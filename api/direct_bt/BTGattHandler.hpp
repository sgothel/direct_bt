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

#ifndef BT_GATT_HANDLER_HPP_
#define BT_GATT_HANDLER_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>

#include <mutex>
#include <atomic>
#include <thread>

#include <jau/environment.hpp>
#include <jau/ringbuffer.hpp>
#include <jau/cow_darray.hpp>
#include <jau/uuid.hpp>
#include <jau/service_runner.hpp>

#include "BTTypes0.hpp"
#include "L2CAPComm.hpp"
#include "ATTPDUTypes.hpp"
#include "GattNumbers.hpp"
#include "DBGattServer.hpp"

/**
 * - - - - - - - - - - - - - - -
 *
 * Module BTGattHandler:
 *
 * - BT Core Spec v5.2: Vol 3, Part G Generic Attribute Protocol (GATT)
 * - BT Core Spec v5.2: Vol 3, Part G GATT: 2.6 GATT Profile Hierarchy
 * - BT Core Spec v5.2: Vol 3, Part G GATT: 3.4 Summary of GATT Profile Attribute Types
 */
namespace direct_bt {

    class BTDevice; // forward
    typedef std::shared_ptr<BTDevice> BTDeviceRef;

    /**
     * GATT Singleton runtime environment properties
     * <p>
     * Also see {@link DBTEnv::getExplodingProperties(const std::string & prefixDomain)}.
     * </p>
     */
    class BTGattEnv : public jau::root_environment {
        private:
            BTGattEnv() noexcept;

            const bool exploding; // just to trigger exploding properties

        public:
            /**
             * Timeout for GATT read command replies, defaults to 550ms minimum,
             * where 500ms is the minimum supervising timeout HCIConstInt::LE_CONN_MIN_TIMEOUT_MS.
             *
             * Environment variable is 'direct_bt.gatt.cmd.read.timeout'.
             *
             * Actually used timeout will be `max(connection_supervisor_timeout + 50ms, GATT_READ_COMMAND_REPLY_TIMEOUT)`,
             * additional 50ms to allow L2CAP timeout hit first.
             */
            const jau::fraction_i64 GATT_READ_COMMAND_REPLY_TIMEOUT;

            /**
             * Timeout for GATT write command replies, defaults to 550ms minimum,
             * where 500ms is the minimum supervising timeout HCIConstInt::LE_CONN_MIN_TIMEOUT_MS.
             *
             * Environment variable is 'direct_bt.gatt.cmd.write.timeout'.
             *
             * Actually used timeout will be `max(connection_supervisor_timeout + 50ms, GATT_WRITE_COMMAND_REPLY_TIMEOUT)`,
             * additional 50ms to allow L2CAP timeout hit first.
             */
            const jau::fraction_i64 GATT_WRITE_COMMAND_REPLY_TIMEOUT;

            /**
             * Timeout for l2cap _initial_ command reply, defaults to 2500ms (2000ms minimum).
             *
             * Environment variable is 'direct_bt.gatt.cmd.init.timeout'.
             *
             * Actually used timeout will be `min(10000, max(2 * connection_supervisor_timeout, GATT_INITIAL_COMMAND_REPLY_TIMEOUT))`,
             * double of connection_supervisor_timeout, to make sure L2CAP timeout hits first.
             */
            const jau::fraction_i64 GATT_INITIAL_COMMAND_REPLY_TIMEOUT;

            /**
             * Medium ringbuffer capacity, defaults to 128 messages.
             * <p>
             * Environment variable is 'direct_bt.gatt.ringsize'.
             * </p>
             */
            const int32_t ATTPDU_RING_CAPACITY;

            /**
             * Debug all GATT Data communication
             * <p>
             * Environment variable is 'direct_bt.debug.gatt.data'.
             * </p>
             */
            const bool DEBUG_DATA;

        public:
            static BTGattEnv& get() noexcept {
                /**
                 * Thread safe starting with C++11 6.7:
                 *
                 * If control enters the declaration concurrently while the variable is being initialized,
                 * the concurrent execution shall wait for completion of the initialization.
                 *
                 * (Magic Statics)
                 *
                 * Avoiding non-working double checked locking.
                 */
                static BTGattEnv e;
                return e;
            }
    };

    /**
     * A thread safe GATT handler associated to one device via one L2CAP connection.
     *
     * Implementation utilizes a lock free ringbuffer receiving data within its separate thread.
     *
     * Controlling Environment variables, see {@link BTGattEnv}.
     *
     * @anchor BTGattHandlerRoles
     * Local GATTRole to a remote BTDevice, (see getRole()):
     *
     * - {@link GATTRole::Server}: The remote device in ::BTRole::Master role running a ::GATTRole::Client. We acts as a ::GATTRole::Server.
     * - {@link GATTRole::Client}: The remote device in ::BTRole::Slave role running a ::GATTRole::Server. We acts as a ::GATTRole::Client.
     *
     * See [BTDevice roles](@ref BTDeviceRoles) and [BTAdapter roles](@ref BTAdapterRoles).
     *
     * @see GATTRole
     * @see [BTAdapter roles](@ref BTAdapterRoles).
     * @see [BTDevice roles](@ref BTDeviceRoles).
     * @see [BTGattHandler roles](@ref BTGattHandlerRoles).
     * @see [Bluetooth Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
     */
    class BTGattHandler {
        public:
            enum class Defaults : uint16_t {
                /**
                 * BT Core Spec v5.2: Vol 3, Part F 3.2.8: Maximum length of an attribute value.
                 *
                 * We add +1 for opcode, but don't add for different PDU type's parameter
                 * upfront the attribute value.
                 */
                MAX_ATT_MTU = 512 + 1,

                /* BT Core Spec v5.2: Vol 3, Part G GATT: 5.2.1 ATT_MTU */
                MIN_ATT_MTU = 23
            };
            static constexpr uint16_t number(const Defaults d) { return static_cast<uint16_t>(d); }

            /** Supervison timeout of the connection. */
            const int32_t supervision_timeout;
            /** Environment runtime configuration, usually used internally only. */
            const BTGattEnv & env;
            /** Derived environment runtime configuration, usually used internally only. */
            const jau::fraction_i64 read_cmd_reply_timeout;
            /** Derived environment runtime configuration, usually used internally only. */
            const jau::fraction_i64 write_cmd_reply_timeout;

            /**
             * Internal handler implementation for given DBGattServer instance
             * matching its DBGattServer::Mode.
             *
             * The specific implementation acts upon GATT requests from a connected client
             * according to DBGattServer::Mode.
             */
            class GattServerHandler {
                public:
                    virtual ~GattServerHandler() { close(); }

                    /**
                     * Close and clear this handler, i.e. release all resources.
                     *
                     * Usually called when disconnected or destructed.
                     */
                    virtual void close() noexcept {}

                    virtual DBGattServer::Mode getMode() noexcept = 0;

                    /**
                     * Reply to an exchange MTU request
                     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.3.1 Exchange MTU (Server configuration)
                     * @param pdu
                     * @return true if transmission was successful, otherwise false
                     */
                    virtual bool replyExchangeMTUReq(const AttExchangeMTU * pdu) noexcept = 0;

                    /**
                     * Reply to a read request
                     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.1 Read Characteristic Value
                     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.3 Read Long Characteristic Value
                     * - For any follow up request, which previous request reply couldn't fit in ATT_MTU (Long Write)
                     * @param pdu
                     * @return true if transmission was successful, otherwise false
                     */
                    virtual bool replyReadReq(const AttPDUMsg * pdu) noexcept = 0;

                    /**
                     * Reply to a write request.
                     *
                     * Without Response:
                     * - BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.5.3 ATT_WRITE_CMD
                     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.1 Write Characteristic Value without Response
                     *
                     * With Response:
                     * - BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.5.1 ATT_WRITE_REQ
                     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
                     * - BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
                     *
                     * - BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.5.2 ATT_WRITE_RSP
                     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
                     *
                     * @param pdu
                     * @return true if transmission was successful, otherwise false
                     */
                    virtual bool replyWriteReq(const AttPDUMsg * pdu) noexcept = 0;

                    /**
                     * Reply to a find info request
                     * - BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.3.1 ATT_FIND_INFORMATION_REQ
                     * - BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.3.2 ATT_FIND_INFORMATION_RSP
                     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.7.1 Discover All Characteristic Descriptors
                     *
                     * @param pdu
                     * @return true if transmission was successful, otherwise false
                     */
                    virtual bool replyFindInfoReq(const AttFindInfoReq * pdu) noexcept = 0;

                    /**
                     * Reply to a find by type value request
                     * - BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.3.3 ATT_FIND_BY_TYPE_VALUE_REQ
                     * - BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.3.4 ATT_FIND_BY_TYPE_VALUE_RSP
                     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.4.2 Discover Primary Service by Service UUID
                     * @param pdu
                     * @return true if transmission was successful, otherwise false
                     */
                    virtual bool replyFindByTypeValueReq(const AttFindByTypeValueReq * pdu) noexcept = 0;

                    /**
                     * Reply to a read by type request
                     * - BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.1 ATT_READ_BY_TYPE_REQ
                     * - BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.2 ATT_READ_BY_TYPE_RSP
                     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.6.1 Discover All Characteristics of a Service
                     *
                     * @param pdu
                     * @return true if transmission was successful, otherwise false
                     */
                    virtual bool replyReadByTypeReq(const AttReadByNTypeReq * pdu) noexcept = 0;

                    /**
                     * Reply to a read by group type request
                     * - BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.9 ATT_READ_BY_GROUP_TYPE_REQ
                     * - BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.10 ATT_READ_BY_GROUP_TYPE_RSP
                     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.4.1 Discover All Primary Services
                     *
                     * @param pdu
                     * @return true if transmission was successful, otherwise false
                     */
                    virtual bool replyReadByGroupTypeReq(const AttReadByNTypeReq * pdu) noexcept = 0;
            };

            /**
             * Native GATT characteristic event listener for notification and indication events received from a GATT server.
             */
            class NativeGattCharListener {
                public:
                    struct Section {
                        /** start point, inclusive */
                        uint16_t start;
                        /** end point, exclusive */
                        uint16_t end;

                        Section(uint16_t s, uint16_t e) : start(s), end(e) {}

                        std::string toString() { return "["+std::to_string(start)+".."+std::to_string(end-1)+"]"; }
                    };

                    /**
                     * Called from native BLE stack, initiated by a received notification.
                     * @param source BTDevice origin of this notification
                     * @param charHandle the GATT characteristic handle related to this notification
                     * @param charValue the notification value
                     * @param timestamp monotonic timestamp at reception, see jau::getCurrentMilliseconds()
                     */
                    virtual void notificationReceived(BTDeviceRef source, const uint16_t charHandle,
                                                      const jau::TROOctets& charValue, const uint64_t timestamp) = 0;

                    /**
                     * Called from native BLE stack, initiated by a received indication.
                     * @param source BTDevice origin of this indication
                     * @param charHandle the GATT characteristic handle related to this indication
                     * @param charValue the indication value
                     * @param timestamp monotonic timestamp at reception, see jau::getCurrentMilliseconds()
                     * @param confirmationSent if true, the native stack has sent the confirmation, otherwise user is required to do so.
                     */
                    virtual void indicationReceived(BTDeviceRef source, const uint16_t charHandle,
                                                    const jau::TROOctets& charValue, const uint64_t timestamp,
                                                    const bool confirmationSent) = 0;

                    /**
                     * Informal low-level notification of AttPDUMsg requests to this GATTRole::Server, optional
                     *
                     * @param pduRequest the request
                     * @param serverDest the GATTRole::Server receiver device, never nullptr
                     * @param clientSource the GATTRole::Client source device, only known and not nullptr for DBGattServer::Mode:FWD GattServerHandler
                     */
                    virtual void requestSent([[maybe_unused]] const AttPDUMsg& pduRequest,
                                             [[maybe_unused]] BTDeviceRef serverDest,
                                             [[maybe_unused]] BTDeviceRef clientSource) { }

                    /**
                     * Informal low-level notification of AttPDUMsg responses from this GATTRole::Server, optional.
                     *
                     * @param pduReply the response
                     * @param serverSource the GATTRole::Server source device, never nullptr
                     * @param clientDest the GATTRole::Client receiver device, only known and not nullptr for DBGattServer::Mode:FWD GattServerHandler
                     */
                    virtual void replyReceived([[maybe_unused]] const AttPDUMsg& pduReply,
                                               [[maybe_unused]] BTDeviceRef serverSource,
                                               [[maybe_unused]] BTDeviceRef clientDest) { }

                    /**
                     * Informal notification about a complete MTU exchange request and response to and from this GATTRole::Server, optional.
                     *
                     * @param clientMTU the client MTU request
                     * @param pduReply the response
                     * @param error_reply in case of an AttErrorRsp reply, the AttErrorRsp::ErrorCode is passed for convenience, otherwise AttErrorRsp::ErrorCode::NO_ERROR.
                     * @param serverMTU the replied server MTU, passed for convenience
                     * @param usedMTU the MTU minimum of client and server to be used, passed for convenience
                     * @param serverReplier the GATTRole::Server replier device, never nullptr
                     * @param clientRequester the GATTRole::Client requester device, only known and not nullptr for DBGattServer::Mode:FWD GattServerHandler
                     */
                    virtual void mtuResponse([[maybe_unused]] const uint16_t clientMTU,
                                             [[maybe_unused]] const AttPDUMsg& pduReply,
                                             [[maybe_unused]] const AttErrorRsp::ErrorCode error_reply,
                                             [[maybe_unused]] const uint16_t serverMTU,
                                             [[maybe_unused]] const uint16_t usedMTU,
                                             [[maybe_unused]] BTDeviceRef serverReplier,
                                             [[maybe_unused]] BTDeviceRef clientRequester) { }
                    /**
                     * Informal notification about a completed write request sent to this GATTRole::Server, optional.
                     *
                     * @param handle the GATT characteristic or descriptor handle, requested to be written
                     * @param data the data requested to be written
                     * @param sections list of NativeGattCharListener::Section within given data, requested to be written. Overlapping consecutive sections have already been merged.
                     * @param with_response true if the write requests expects a response, i.e. via AttPDUMsg::Opcode::WRITE_REQ or AttPDUMsg::Opcode::EXECUTE_WRITE_REQ
                     * @param serverDest the GATTRole::Server receiver device, never nullptr
                     * @param clientSource the GATTRole::Client source device, only known and not nullptr for DBGattServer::Mode:FWD GattServerHandler
                     */
                    virtual void writeRequest([[maybe_unused]] const uint16_t handle,
                                              [[maybe_unused]] const jau::TROOctets& data,
                                              [[maybe_unused]] const jau::darray<Section>& sections,
                                              [[maybe_unused]] const bool with_response,
                                              [[maybe_unused]] BTDeviceRef serverDest,
                                              [[maybe_unused]] BTDeviceRef clientSource) { }

                    /**
                     * Informal notification about a write response received from this GATTRole::Server, optional.
                     *
                     * @param pduReply the write response
                     * @param error_code in case of an AttErrorRsp reply, the AttErrorRsp::ErrorCode is passed for convenience, otherwise AttErrorRsp::ErrorCode::NO_ERROR.
                     * @param serverSource the GATTRole::Server source device, never nullptr
                     * @param clientDest the GATTRole::Client receiver device, only known and not nullptr for DBGattServer::Mode:FWD GattServerHandler
                     */
                    virtual void writeResponse([[maybe_unused]] const AttPDUMsg& pduReply,
                                               [[maybe_unused]] const AttErrorRsp::ErrorCode error_code,
                                               [[maybe_unused]] BTDeviceRef serverSource,
                                               [[maybe_unused]] BTDeviceRef clientDest) { }


                    /**
                     * Informal notification about a complete read request and response to and from this GATTRole::Server, optional.
                     *
                     * @param handle the GATT characteristic or descriptor handle, requested to be written
                     * @param value_offset the value offset of the data to be read
                     * @param pduReply the response
                     * @param error_reply in case of an AttErrorRsp reply, the AttErrorRsp::ErrorCode is passed for convenience, otherwise AttErrorRsp::ErrorCode::NO_ERROR.
                     * @param data_reply the replied read data at given value_offset, passed for convenience
                     * @param serverReplier the GATTRole::Server replier device, never nullptr
                     * @param clientRequester the GATTRole::Client requester device, only known and not nullptr for DBGattServer::Mode:FWD GattServerHandler
                     */
                    virtual void readResponse([[maybe_unused]] const uint16_t handle,
                                              [[maybe_unused]] const uint16_t value_offset,
                                              [[maybe_unused]] const AttPDUMsg& pduReply,
                                              [[maybe_unused]] const AttErrorRsp::ErrorCode error_reply,
                                              [[maybe_unused]] const jau::TROOctets& data_reply,
                                              [[maybe_unused]] BTDeviceRef serverReplier,
                                              [[maybe_unused]] BTDeviceRef clientRequester) { }

                    virtual ~NativeGattCharListener() noexcept {}

                    /** Return a simple description about this instance. */
                    virtual std::string toString() {
                        return "NativeGattCharListener["+jau::to_string(this)+"]";
                    }

                    /**
                     * Default comparison operator, merely testing for same memory reference.
                     * <p>
                     * Specializations may override.
                     * </p>
                     */
                    virtual bool operator==(const NativeGattCharListener& rhs) const noexcept
                    { return this == &rhs; }

                    bool operator!=(const NativeGattCharListener& rhs) const noexcept
                    { return !(*this == rhs); }
            };
            typedef std::shared_ptr<NativeGattCharListener> NativeGattCharListenerRef;
            typedef jau::cow_darray<NativeGattCharListenerRef> NativeGattCharListenerList_t;
            typedef jau::darray<NativeGattCharListener::Section> NativeGattCharSections_t;

       private:
            /** BTGattHandler's device weak back-reference */
            std::weak_ptr<BTDevice> wbr_device;
            GATTRole role;
            L2CAPClient& l2cap;

            const std::string deviceString;
            mutable std::recursive_mutex mtx_command;
            jau::POctets rbuffer;

            jau::sc_atomic_bool is_connected; // reflects state
            jau::relaxed_atomic_bool has_ioerror;  // reflects state

            jau::service_runner l2cap_reader_service;
            jau::ringbuffer<std::unique_ptr<const AttPDUMsg>, jau::nsize_t> attPDURing;

            jau::relaxed_atomic_uint16 serverMTU; // set in initClientGatt()
            jau::relaxed_atomic_uint16 usedMTU; // concurrent use in initClientGatt(set), send and l2capReaderThreadImpl
            jau::relaxed_atomic_bool clientMTUExchanged; // set in initClientGatt()

            /** send immediate confirmation of indication events from device, defaults to true. */
            jau::relaxed_atomic_bool sendIndicationConfirmation = true;

            struct GattCharListenerPair {
                /** The actual listener */
                BTGattCharListenerRef listener;
                /** The optional weak device reference. Weak, b/c it shall not block destruction */
                std::weak_ptr<BTGattChar> wbr_characteristic;

                bool match(const BTGattChar& characteristic) const noexcept {
                    BTGattCharRef sda = wbr_characteristic.lock();
                    if( nullptr != sda ) {
                        return *sda == characteristic;
                    } else {
                        return true;
                    }
                }
            };
            typedef jau::cow_darray<GattCharListenerPair> gattCharListenerList_t;
            static gattCharListenerList_t::equal_comparator gattCharListenerRefEqComparator;
            gattCharListenerList_t gattCharListenerList;

            NativeGattCharListenerList_t nativeGattCharListenerList;

            /** Pass through user Gatt-Server database, non-nullptr if ::GATTRole::Server */
            DBGattServerRef gattServerData;
            /** Always set, never nullptr */
            std::unique_ptr<GattServerHandler> gattServerHandler;
            static std::unique_ptr<GattServerHandler> selectGattServerHandler(BTGattHandler& gh, DBGattServerRef gattServerData) noexcept;

            jau::darray<BTGattServiceRef> services;
            std::shared_ptr<GattGenericAccessSvc> genericAccess = nullptr;

            bool validateConnected() noexcept;

            DBGattCharRef findServerGattCharByValueHandle(const uint16_t char_value_handle) noexcept;

            /**
             * Reply given ATT message using the installed GattServerHandler gattServerHandler
             * @param pdu the message
             * @return true if transmission was successful, otherwise false
             */
            bool replyAttPDUReq(std::unique_ptr<const AttPDUMsg> && pdu) noexcept;

            void l2capReaderWork(jau::service_runner& sr) noexcept;
            void l2capReaderEndLocked(jau::service_runner& sr) noexcept;

            bool l2capReaderInterrupted(int dummy=0) /* const */ noexcept;

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.4.2 MTU Exchange
             *
             * Returns the server-mtu if successful, otherwise 0.
             *
             * Method usually called via initClientGatt() and is only exposed special applications.
             *
             * @see initClientGatt()
             */
            uint16_t clientMTUExchange(const jau::fraction_i64& timeout) noexcept;

            /**
             * Discover all primary services _only_.
             * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.4.1 Discover All Primary Services
             *
             * @param shared_this shared pointer of this instance, used to forward a weak_ptr to BTGattService for back-reference. Reference is validated.
             * @param result vector containing all discovered primary services
             * @return true on success, otherwise false
             * @see initClientGatt()
             * @see discoverCompletePrimaryServices()
             */
            bool discoverPrimaryServices(std::shared_ptr<BTGattHandler> shared_this, jau::darray<BTGattServiceRef> & result) noexcept;

            /**
             * Discover all characteristics of a service and declaration attributes _only_.
             * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.6.1 Discover All Characteristics of a Service
             * - BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.1 Characterisic Declaration Attribute Value
             *
             * @see initClientGatt()
             * @see discoverCompletePrimaryServices()
             */
            bool discoverCharacteristics(BTGattServiceRef & service) noexcept;

            /**
             * Discover all descriptors of a service _only_.
             * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.7.1 Discover All Characteristic Descriptors
             *
             * @see initClientGatt()
             * @see discoverCompletePrimaryServices()
             */
            bool discoverDescriptors(BTGattServiceRef & service) noexcept;

            /**
             * Discover all primary services _and_ all its characteristics declarations
             * including their client config.
             * <p>
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.4.1 Discover All Primary Services
             * </p>
             * Populates the internal internal BTGattService vector of discovered services.
             *
             * Service discovery may consume 500ms - 2000ms, depending on bandwidth.
             *
             * Method called from initClientGatt().
             *
             * @param shared_this shared pointer of this instance, used to forward a weak_ptr to BTGattService for back-reference. Reference is validated.
             * @return true if successful, otherwise false
             * @see initClientGatt()
             */
            bool discoverCompletePrimaryServices(std::shared_ptr<BTGattHandler> shared_this) noexcept;

        public:
            /**
             * Constructing a new BTGattHandler instance with its opened and connected L2CAP channel.
             * <p>
             * After successful l2cap connection, the MTU will be exchanged.
             * See getServerMTU() and getUsedMTU(), the latter is in use.
             * </p>
             * @param device the connected remote device
             * @param l2cap_att the underlying used L2CAP
             * @param supervision_timeout the connection supervising timeout in [ms]
             */
            BTGattHandler(const BTDeviceRef & device, L2CAPClient& l2cap_att, const int32_t supervision_timeout) noexcept;

            BTGattHandler(const BTGattHandler&) = delete;
            void operator=(const BTGattHandler&) = delete;

            /** Destructor closing this instance including L2CAP channel, see {@link #disconnect()}. */
            ~BTGattHandler() noexcept;

            BTDeviceRef getDeviceUnchecked() const noexcept { return wbr_device.lock(); }
            BTDeviceRef getDeviceChecked() const;

            /**
             * Return the local GATTRole to the remote BTDevice.
             * @see GATTRole
             * @see [BTGattHandler roles](@ref BTGattHandlerRoles).
             * @since 2.4.0
             */
            GATTRole getRole() const noexcept { return role; }

            bool isConnected() const noexcept { return is_connected ; }
            bool hasIOError() const noexcept { return has_ioerror; }
            std::string getStateString() const noexcept;

            /**
             * Disconnect this BTGattHandler and optionally the associated device
             * @param disconnect_device if true, associated device will also be disconnected, otherwise not.
             * @param ioerr_cause if true, reason for disconnection is an IO error
             * @return true if successful, otherwise false
             */
            bool disconnect(const bool disconnect_device, const bool ioerr_cause) noexcept;

            inline uint16_t getServerMTU() const noexcept { return serverMTU; }
            inline uint16_t getUsedMTU()  const noexcept { return usedMTU; }
            void setUsedMTU(const uint16_t mtu) noexcept { usedMTU = mtu; }

            /**
             * Find and return the BTGattChar within given list of primary services
             * via given characteristic value handle.
             * <p>
             * Returns nullptr if not found.
             * </p>
             */
            BTGattCharRef findCharacterisicsByValueHandle(const jau::darray<BTGattServiceRef> &services_, const uint16_t charValueHandle) noexcept;

            /**
             * Find and return the BTGattChar within given primary service
             * via given characteristic value handle.
             * <p>
             * Returns nullptr if not found.
             * </p>
             */
            BTGattCharRef findCharacterisicsByValueHandle(const BTGattServiceRef service, const uint16_t charValueHandle) noexcept;

            /**
             * Initialize the connection and internal data set for GATT client operations:
             * - Exchange MTU
             * - Discover all primary services, its characteristics and its descriptors
             * - Extracts the GattGenericAccessSvc from the services, see getGenericAccess()
             *
             * Service discovery may consume 500ms - 2000ms, depending on bandwidth.
             *
             * @param shared_this the shared BTGattHandler reference
             * @param already_init if already initialized true, will hold true, otherwise false
             * @return true if already initialized or successfully newly initialized with at least GattGenericAccessSvc available, otherwise false
             * @see clientMTUExchange()
             * @see discoverCompletePrimaryServices()
             */
            bool initClientGatt(std::shared_ptr<BTGattHandler> shared_this, bool& already_init) noexcept;

            /**
             * Returns a reference of the internal kept BTGattService list.
             *
             * The internal list should have been populated via initClientGatt() once.
             *
             * @see initClientGatt()
             */
            inline jau::darray<BTGattServiceRef> & getServices() noexcept { return services; }

            /**
             * Returns the internal kept shared GattGenericAccessSvc instance.
             *
             * This instance is created via initClientGatt().
             *
             * @see initClientGatt()
             */
            inline std::shared_ptr<GattGenericAccessSvc> getGenericAccess() noexcept { return genericAccess; }

            /**
             * Sends the given AttPDUMsg to the connected device via l2cap.
             *
             * ATT_MTU range
             * - ATT_MTU minimum is 23 bytes (Vol 3, Part G: 5.2.1)
             * - ATT_MTU is negotiated, maximum attribute value length is 512 bytes (Vol 3, Part F: 3.2.8-9)
             * - ATT Value sent: [1 .. ATT_MTU-1] (Vol 3, Part F: 3.2.8-9)
             *
             * Implementation disconnect() and returns false if an unexpected l2cap write errors occurs.
             *
             * @param msg the message to be send
             * @return true if successful otherwise false if write error, not connected or if message size exceeds usedMTU-1.
             */
            bool send(const AttPDUMsg & msg) noexcept;

            /**
             * Sends the given AttPDUMsg to the connected device via l2cap using {@link #send()}.
             *
             * Implementation waits for timeout milliseconds receiving the response
             * from the ringbuffer, filled from the reader-thread.
             *
             * Implementation disconnect() and returns nullptr
             * if no matching reply has been received within timeout milliseconds.
             *
             * In case method completes successfully,
             * the message has been send out and a reply has also been received and is returned as the result.
             *
             * @param msg the message to be send
             * @param timeout fractions of seconds to wait for a reply
             * @return non nullptr for a valid reply, otherwise nullptr
             */
            std::unique_ptr<const AttPDUMsg> sendWithReply(const AttPDUMsg & msg, const jau::fraction_i64& timeout) noexcept;

            /**
             * Generic read GATT value and long value
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
             */
            bool readValue(const uint16_t handle, jau::POctets & res, int expectedLength=-1) noexcept;

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
             */
            bool readCharacteristicValue(const BTGattChar & c, jau::POctets & res, int expectedLength=-1) noexcept;

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
             */
            bool readDescriptorValue(BTGattDesc & cd, int expectedLength=-1) noexcept;

            /**
             * Generic write GATT value and long value
             */
            bool writeValue(const uint16_t handle, const jau::TROOctets & value, const bool withResponse) noexcept;

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.12.3 Write Characteristic Descriptors
             * <p>
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3 Characteristic Descriptor
             * </p>
             * <p>
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
             * </p>
             */
            bool writeDescriptorValue(const BTGattDesc & cd) noexcept;

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
             */
            bool writeCharacteristicValue(const BTGattChar & c, const jau::TROOctets & value) noexcept;

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.1 Write Characteristic Value Without Response
             */
            bool writeCharacteristicValueNoResp(const BTGattChar & c, const jau::TROOctets & value) noexcept;

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
             * <p>
             * Method enables notification and/or indication for the corresponding characteristic at BLE level.
             * </p>
             * <p>
             * It is recommended to utilize notification over indication, as its link-layer handshake
             * and higher potential bandwidth may deliver material higher performance.
             * </p>
             * <p>
             * Throws an IllegalArgumentException if the given BTGattDesc is not a ClientCharacteristicConfiguration.
             * </p>
             */
            bool configNotificationIndication(BTGattDesc & cd, const bool enableNotification, const bool enableIndication) noexcept;

            /**
             * Send a notification event consisting out of the given `value` representing the given characteristic value handle
             * to the connected BTRole::Master.
             *
             * This command is only valid if this BTGattHandler is in role GATTRole::Server.
             *
             * Implementation is not receiving any reply after sending out the indication and returns immediately.
             *
             * @param char_value_handle valid characteristic value handle, must be sourced from referenced DBGattServer
             * @return true if successful, otherwise false
             */
            bool sendNotification(const uint16_t char_value_handle, const jau::TROOctets & value) noexcept;

            /**
             * Send an indication event consisting out of the given `value` representing the given characteristic value handle
             * to the connected BTRole::Master.
             *
             * This command is only valid if this BTGattHandler is in role GATTRole::Server.
             *
             * Implementation awaits the indication reply after sending out the indication.
             *
             * @param char_value_handle valid characteristic value handle, must be sourced from referenced DBGattServer
             * @return true if successful, otherwise false
             */
            bool sendIndication(const uint16_t char_value_handle, const jau::TROOctets & value) noexcept;

            /**
             * Add the given listener to the list if not already present.
             * <p>
             * Returns true if the given listener is not element of the list and has been newly added,
             * otherwise false.
             * </p>
             */
            bool addCharListener(const BTGattCharListenerRef& l) noexcept;

            /**
             * Please use BTGattChar::addCharListener() for clarity, merely existing here to allow JNI access.
             */
            bool addCharListener(const BTGattCharListenerRef& l, const BTGattCharRef& d) noexcept;

            /**
             * Remove the given listener from the list.
             * <p>
             * Returns true if the given listener is an element of the list and has been removed,
             * otherwise false.
             * </p>
             */
            bool removeCharListener(const BTGattCharListenerRef& l) noexcept;

            /**
             * Remove the given listener from the list.
             * <p>
             * Returns true if the given listener is an element of the list and has been removed,
             * otherwise false.
             * </p>
             */
            bool removeCharListener(const BTGattCharListener * l) noexcept;
            
            /**
             * Remove all {@link BTGattCharListener} from the list, which are associated to the given {@link BTGattChar}
             * when added via BTGattChar::addCharListener().
             *
             * @param associatedCharacteristic the match criteria to remove any BTGattCharListener from the list
             * @return number of removed listener.
             */
            int removeAllAssociatedCharListener(const BTGattCharRef& associatedChar) noexcept;

            int removeAllAssociatedCharListener(const BTGattChar * associatedChar) noexcept;

            /**
             * Add the given listener to the list if not already present.
             * <p>
             * Returns true if the given listener is not element of the list and has been newly added,
             * otherwise false.
             * </p>
             */
            bool addCharListener(const NativeGattCharListenerRef& l) noexcept;

            /**
             * Remove the given listener from the list.
             * <p>
             * Returns true if the given listener is an element of the list and has been removed,
             * otherwise false.
             * </p>
             */
            bool removeCharListener(const NativeGattCharListenerRef& l) noexcept;

            /**
             * Remove all event listener from the list.
             * <p>
             * Returns the number of removed event listener.
             * </p>
             */
            int removeAllCharListener() noexcept;

            /**
             * Return event listener count.
             */
            jau::nsize_t getCharListenerCount() const noexcept { return gattCharListenerList.size() + nativeGattCharListenerList.size(); }

            /**
             * Print a list of all BTGattCharListener and NativeGattCharListener.
             *
             * This is merely a facility for debug and analysis.
             */
            void printCharListener() noexcept;

            /**
             * Notify all NativeGattCharListener about a low-level AttPDUMsg request being sent to this GATTRole::Server.
             *
             * This functionality has an informal character only.
             *
             * @param pduRequest the request
             * @param clientSource the GATTRole::Client source device, only known and not nullptr for DBGattServer::Mode:FWD GattServerHandler
             */
            void notifyNativeRequestSent(const AttPDUMsg& pduRequest, const BTDeviceRef& clientSource) noexcept;

            /**
             * Notify all NativeGattCharListener about a low-level AttPDUMsg reply being received from this GATTRole::Server.
             *
             * @param pduReply the response
             * @param clientDest the GATTRole::Client receiver device, only known and not nullptr for DBGattServer::Mode:FWD GattServerHandler
             */
            void notifyNativeReplyReceived(const AttPDUMsg& pduReply, const BTDeviceRef& clientDest) noexcept;

            /**
             * Notify all NativeGattCharListener about a completed MTU exchange request and response to and from this GATTRole::Server.
             *
             * This functionality has an informal character only.
             *
             * @param clientMTU the client MTU request
             * @param pduReply the response
             * @param error_reply in case of an AttErrorRsp reply, the AttErrorRsp::ErrorCode is passed for convenience, otherwise AttErrorRsp::ErrorCode::NO_ERROR.
             * @param serverMTU the replied server MTU, passed for convenience
             * @param usedMTU the MTU minimum of client and server to be used, passed for convenience
             * @param clientRequester the GATTRole::Client requester device, only known and not nullptr for DBGattServer::Mode:FWD GattServerHandler
             */
            void notifyNativeMTUResponse(const uint16_t clientMTU,
                                         const AttPDUMsg& pduReply, const AttErrorRsp::ErrorCode error_reply,
                                         const uint16_t serverMTU, const uint16_t usedMTU,
                                         const BTDeviceRef& clientRequester) noexcept;

            /**
             * Notify all NativeGattCharListener about a completed write request sent to this GATTRole::Server.
             *
             * This functionality has an informal character only.
             *
             * @param handle the GATT characteristic or descriptor handle, requested to be written
             * @param data the data requested to be written
             * @param sections list of NativeGattCharListener::Section within given data, requested to be written. Overlapping consecutive sections have already been merged.
             * @param with_response true if the write requests expects a response, i.e. via AttPDUMsg::Opcode::WRITE_REQ or AttPDUMsg::Opcode::EXECUTE_WRITE_REQ
             * @param clientSource the GATTRole::Client source device, only known and not nullptr for DBGattServer::Mode:FWD GattServerHandler
             */
            void notifyNativeWriteRequest(const uint16_t handle, const jau::TROOctets& data, const NativeGattCharSections_t& sections, const bool with_response, const BTDeviceRef& clientSource) noexcept;

            /**
             * Notify all NativeGattCharListener about a write response received from this GATTRole::Server.
             *
             * This functionality has an informal character only.
             *
             * @param pduReply the response
             * @param error_code in case of an AttErrorRsp reply, the AttErrorRsp::ErrorCode is passed for convenience, otherwise AttErrorRsp::ErrorCode::NO_ERROR.
             * @param clientDest the GATTRole::Client receiver device, only known and not nullptr for DBGattServer::Mode:FWD GattServerHandler
             */
            void notifyNativeWriteResponse(const AttPDUMsg& pduReply, const AttErrorRsp::ErrorCode error_code, const BTDeviceRef& clientDest) noexcept;

            /**
             * Notify all NativeGattCharListener about a completed read request and response to and from this GATTRole::Server.
             *
             * This functionality has an informal character only.
             *
             * @param handle the GATT characteristic or descriptor handle, requested to be written
             * @param value_offset the value offset of the data to be read
             * @param pduReply the response
             * @param error_reply in case of an AttErrorRsp reply, the AttErrorRsp::ErrorCode is passed for convenience, otherwise AttErrorRsp::ErrorCode::NO_ERROR.
             * @param data_reply the replied read data at given value_offset, passed for convenience
             * @param clientRequester the GATTRole::Client requester device, only known and not nullptr for DBGattServer::Mode:FWD GattServerHandler
             */
            void notifyNativeReadResponse(const uint16_t handle, const uint16_t value_offset,
                                          const AttPDUMsg& pduReply, const AttErrorRsp::ErrorCode error_reply, const jau::TROOctets& data_reply,
                                          const BTDeviceRef& clientRequester) noexcept;

            /**
             * Enable or disable sending an immediate confirmation for received indication events from the device.
             * <p>
             * Default value is true.
             * </p>
             * <p>
             * This setting is per BTGattHandler and hence per BTDevice.
             * </p>
             */
            void setSendIndicationConfirmation(const bool v) noexcept;

            /**
             * Returns whether sending an immediate confirmation for received indication events from the device is enabled.
             * <p>
             * Default value is true.
             * </p>
             * <p>
             * This setting is per BTGattHandler and hence per BTDevice.
             * </p>
             */
            bool getSendIndicationConfirmation() noexcept;

            /*****************************************************/
            /** Higher level semantic functionality **/
            /*****************************************************/

            std::shared_ptr<GattGenericAccessSvc> getGenericAccess(jau::darray<BTGattServiceRef> & primServices) noexcept;
            std::shared_ptr<GattGenericAccessSvc> getGenericAccess(jau::darray<BTGattCharRef> & genericAccessCharDeclList) noexcept;

            std::shared_ptr<GattDeviceInformationSvc> getDeviceInformation(jau::darray<BTGattServiceRef> & primServices) noexcept;
            std::shared_ptr<GattDeviceInformationSvc> getDeviceInformation(jau::darray<BTGattCharRef> & deviceInfoCharDeclList) noexcept;

            /**
             * Issues a ping to the device, validating whether it is still reachable.
             * <p>
             * This method could be periodically utilized to shorten the underlying OS disconnect period
             * after turning the device off, which lies within 7-13s.
             * </p>
             * <p>
             * In case the device is no more reachable, disconnect will be initiated due to the occurring IO error.
             * </p>
             * @return `true` if successful, otherwise false in case no GATT services exists etc.
             */
            bool ping() noexcept;

            std::string toString() const noexcept;
    };
    typedef std::shared_ptr<BTGattHandler> BTGattHandlerRef;

} // namespace direct_bt

#endif /* BT_GATT_HANDLER_HPP_ */
