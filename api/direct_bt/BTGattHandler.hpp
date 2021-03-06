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

#include "UUID.hpp"
#include "BTTypes0.hpp"
#include "L2CAPComm.hpp"
#include "ATTPDUTypes.hpp"
#include "GattTypes.hpp"

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
             * Timeout for GATT read command replies, defaults to 500ms.
             * <p>
             * Environment variable is 'direct_bt.gatt.cmd.read.timeout'.
             * </p>
             */
            const int32_t GATT_READ_COMMAND_REPLY_TIMEOUT;

            /**
             * Timeout for GATT write command replies, defaults to 500ms.
             * <p>
             * Environment variable is 'direct_bt.gatt.cmd.write.timeout'.
             * </p>
             */
            const int32_t GATT_WRITE_COMMAND_REPLY_TIMEOUT;

            /**
             * Timeout for l2cap _initial_ command reply, defaults to 2500ms.
             * <p>
             * Environment variable is 'direct_bt.gatt.cmd.init.timeout'.
             * </p>
             */
            const int32_t GATT_INITIAL_COMMAND_REPLY_TIMEOUT;

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
     * <p>
     * Implementation utilizes a lock free ringbuffer receiving data within its separate thread.
     * </p>
     * <p>
     * Controlling Environment variables, see {@link BTGattEnv}.
     * </p>
     */
    class BTGattHandler {
        public:
            enum class Defaults : int32_t {
                /* BT Core Spec v5.2: Vol 3, Part F 3.2.8: Maximum length of an attribute value. */
                MAX_ATT_MTU = 512,

                /* BT Core Spec v5.2: Vol 3, Part G GATT: 5.2.1 ATT_MTU */
                MIN_ATT_MTU = 23
            };
            static constexpr int number(const Defaults d) { return static_cast<int>(d); }

       private:
            const BTGattEnv & env;

            /** BTGattHandler's device weak back-reference */
            std::weak_ptr<BTDevice> wbr_device;
            L2CAPComm& l2cap;

            const std::string deviceString;
            std::recursive_mutex mtx_command;
            POctets rbuffer;

            jau::sc_atomic_bool is_connected; // reflects state
            jau::relaxed_atomic_bool has_ioerror;  // reflects state

            jau::ringbuffer<std::unique_ptr<const AttPDUMsg>, std::nullptr_t, jau::nsize_t> attPDURing;
            jau::sc_atomic_bool l2capReaderShallStop;

            std::mutex mtx_l2capReaderLifecycle;
            std::condition_variable cv_l2capReaderInit;
            pthread_t l2capReaderThreadId;
            jau::relaxed_atomic_bool l2capReaderRunning;

            /** send immediate confirmation of indication events from device, defaults to true. */
            jau::relaxed_atomic_bool sendIndicationConfirmation = true;
            typedef jau::cow_darray<std::shared_ptr<BTGattCharListener>> characteristicListenerList_t;
            characteristicListenerList_t characteristicListenerList;

            uint16_t serverMTU;
            std::atomic<uint16_t> usedMTU; // concurrent use in ctor(set), send and l2capReaderThreadImpl
            jau::darray<BTGattServiceRef> services;
            std::shared_ptr<GattGenericAccessSvc> genericAccess = nullptr;

            bool validateConnected() noexcept;

            void l2capReaderThreadImpl();

            /**
             * Sends the given AttPDUMsg to the connected device via l2cap.
             * <p>
             * Implementation throws an IllegalStateException if not connected,
             * a IllegalArgumentException if message size exceeds usedMTU.
             * </p>
             * <p>
             * Implementation disconnect() and throws an BluetoothException
             * if an l2cap write errors occurs.
             * </p>
             * <p>
             * In case method completes, the message has been send out successfully.
             * </p>
             * @param msg the message to be send
             */
            void send(const AttPDUMsg & msg);

            /**
             * Sends the given AttPDUMsg to the connected device via l2cap using {@link #send()}.
             * <p>
             * Implementation waits for timeout milliseconds receiving the response
             * from the ringbuffer, filled from the reader-thread.
             * </p>
             * <p>
             * Implementation disconnect() and throws an BluetoothException
             * if no matching reply has been received within timeout milliseconds.
             * </p>
             * <p>
             * In case method completes, the message has been send out successfully
             * and a reply has also been received and is returned as a result.<br>
             * Hence this method either throws an exception or returns a matching reply.
             * </p>
             *
             * @param msg the message to be send
             * @param timeout milliseconds to wait for a reply
             * @return a valid reply, never nullptrs
             */
            std::unique_ptr<const AttPDUMsg> sendWithReply(const AttPDUMsg & msg, const int timeout);

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.4.2 MTU Exchange
             * <p>
             * Returns the server-mtu if successful, otherwise 0.
             * </p>
             */
            uint16_t exchangeMTUImpl(const uint16_t clientMaxMTU, const int32_t timeout);

        public:
            /**
             * Constructing a new BTGattHandler instance with its opened and connected L2CAP channel.
             * <p>
             * After successful l2cap connection, the MTU will be exchanged.
             * See getServerMTU() and getUsedMTU(), the latter is in use.
             * </p>
             */
            BTGattHandler(const std::shared_ptr<BTDevice> & device, L2CAPComm& l2cap_att) noexcept;

            BTGattHandler(const BTGattHandler&) = delete;
            void operator=(const BTGattHandler&) = delete;

            /** Destructor closing this instance including L2CAP channel, see {@link #disconnect()}. */
            ~BTGattHandler() noexcept;

            std::shared_ptr<BTDevice> getDeviceUnchecked() const noexcept { return wbr_device.lock(); }
            std::shared_ptr<BTDevice> getDeviceChecked() const;

            bool isConnected() const noexcept { return is_connected ; }
            bool hasIOError() const noexcept { return has_ioerror; }
            std::string getStateString() const noexcept { return L2CAPComm::getStateString(is_connected, has_ioerror); }

            /**
             * Disconnect this BTGattHandler and optionally the associated device
             * @param disconnectDevice if true, associated device will also be disconnected, otherwise not.
             * @param ioErrorCause if true, reason for disconnection is an IO error
             * @return true if successful, otherwise false
             */
            bool disconnect(const bool disconnectDevice, const bool ioErrorCause) noexcept;

            inline uint16_t getServerMTU() const noexcept { return serverMTU; }
            inline uint16_t getUsedMTU()  const noexcept { return usedMTU; }

            /**
             * Find and return the BTGattChar within internal primary services
             * via given characteristic value handle.
             * <p>
             * Returns nullptr if not found.
             * </p>
             */
            BTGattCharRef findCharacterisicsByValueHandle(const uint16_t charValueHandle) noexcept;

            /**
             * Find and return the BTGattChar within given list of primary services
             * via given characteristic value handle.
             * <p>
             * Returns nullptr if not found.
             * </p>
             */
            BTGattCharRef findCharacterisicsByValueHandle(const uint16_t charValueHandle, jau::darray<BTGattServiceRef> &services) noexcept;

            /**
             * Find and return the BTGattChar within given primary service
             * via given characteristic value handle.
             * <p>
             * Returns nullptr if not found.
             * </p>
             */
            BTGattCharRef findCharacterisicsByValueHandle(const uint16_t charValueHandle, BTGattServiceRef service) noexcept;

            /**
             * Discover all primary services _and_ all its characteristics declarations
             * including their client config.
             * <p>
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.4.1 Discover All Primary Services
             * </p>
             * Method returns reference to BTGattHandler's internal BTGattService vector of discovered services
             *
             * @param shared_this shared pointer of this instance, used to forward a weak_ptr to BTGattService for back-reference. Reference is validated.
             * @return BTGattHandler's internal BTGattService vector of discovered services
             */
            jau::darray<BTGattServiceRef> & discoverCompletePrimaryServices(std::shared_ptr<BTGattHandler> shared_this);

            /**
             * Returns a reference of the internal kept BTGattService list.
             * <p>
             * The internal list will be populated via {@link #discoverCompletePrimaryServices()}.
             * </p>
             */
            inline jau::darray<BTGattServiceRef> & getServices() noexcept { return services; }

            /**
             * Returns the internal kept shared GattGenericAccessSvc instance.
             * <p>
             * This instance is created via {@link #discoverCompletePrimaryServices()}.
             * </p>
             */
            inline std::shared_ptr<GattGenericAccessSvc> getGenericAccess() noexcept { return genericAccess; }

            /**
             * Discover all primary services _only_.
             * <p>
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.4.1 Discover All Primary Services
             * </p>
             * @param shared_this shared pointer of this instance, used to forward a weak_ptr to BTGattService for back-reference. Reference is validated.
             * @param result vector containing all discovered primary services
             * @return true on success, otherwise false
             */
            bool discoverPrimaryServices(std::shared_ptr<BTGattHandler> shared_this, jau::darray<BTGattServiceRef> & result);

            /**
             * Discover all characteristics of a service and declaration attributes _only_.
             * <p>
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.6.1 Discover All Characteristics of a Service
             * </p>
             * <p>
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.1 Characterisic Declaration Attribute Value
             * </p>
             */
            bool discoverCharacteristics(BTGattServiceRef & service);

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.7.1 Discover All Characteristic Descriptors
             */
            bool discoverDescriptors(BTGattServiceRef & service);

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
            bool readValue(const uint16_t handle, POctets & res, int expectedLength=-1);

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
            bool readCharacteristicValue(const BTGattChar & c, POctets & res, int expectedLength=-1);

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
            bool readDescriptorValue(BTGattDesc & cd, int expectedLength=-1);

            /**
             * Generic write GATT value and long value
             */
            bool writeValue(const uint16_t handle, const TROOctets & value, const bool withResponse);

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.12.3 Write Characteristic Descriptors
             * <p>
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3 Characteristic Descriptor
             * </p>
             * <p>
             * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
             * </p>
             */
            bool writeDescriptorValue(const BTGattDesc & cd);

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
             */
            bool writeCharacteristicValue(const BTGattChar & c, const TROOctets & value);

            /**
             * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.1 Write Characteristic Value Without Response
             */
            bool writeCharacteristicValueNoResp(const BTGattChar & c, const TROOctets & value);

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
            bool configNotificationIndication(BTGattDesc & cd, const bool enableNotification, const bool enableIndication);

            /**
             * Add the given listener to the list if not already present.
             * <p>
             * Returns true if the given listener is not element of the list and has been newly added,
             * otherwise false.
             * </p>
             */
            bool addCharListener(std::shared_ptr<BTGattCharListener> l);

            /**
             * Remove the given listener from the list.
             * <p>
             * Returns true if the given listener is an element of the list and has been removed,
             * otherwise false.
             * </p>
             */
            bool removeCharListener(std::shared_ptr<BTGattCharListener> l) noexcept;

            /**
             * Remove the given listener from the list.
             * <p>
             * Returns true if the given listener is an element of the list and has been removed,
             * otherwise false.
             * </p>
             */
            bool removeCharListener(const BTGattCharListener * l) noexcept;

            
            /**
             * Remove all {@link BTGattCharListener} from the list, which are associated to the given {@link BTGattChar}.
             * <p>
             * Implementation tests all listener's BTGattCharListener::match(const BTGattChar & characteristic)
             * to match with the given associated characteristic.
             * </p>
             * @param associatedCharacteristic the match criteria to remove any BTGattCharListener from the list
             * @return number of removed listener.
             */
            int removeAllAssociatedCharListener(std::shared_ptr<BTGattChar> associatedChar) noexcept;

            int removeAllAssociatedCharListener(const BTGattChar * associatedChar) noexcept;

            /**
             * Remove all event listener from the list.
             * <p>
             * Returns the number of removed event listener.
             * </p>
             */
            int removeAllCharListener() noexcept ;

            /**
             * Enable or disable sending an immediate confirmation for received indication events from the device.
             * <p>
             * Default value is true.
             * </p>
             * <p>
             * This setting is per BTGattHandler and hence per BTDevice.
             * </p>
             */
            void setSendIndicationConfirmation(const bool v);

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

            std::shared_ptr<GattGenericAccessSvc> getGenericAccess(jau::darray<BTGattServiceRef> & primServices);
            std::shared_ptr<GattGenericAccessSvc> getGenericAccess(jau::darray<BTGattCharRef> & genericAccessCharDeclList);

            std::shared_ptr<GattDeviceInformationSvc> getDeviceInformation(jau::darray<BTGattServiceRef> & primServices);
            std::shared_ptr<GattDeviceInformationSvc> getDeviceInformation(jau::darray<BTGattCharRef> & deviceInfoCharDeclList);

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
            bool ping();
    };

} // namespace direct_bt

#endif /* BT_GATT_HANDLER_HPP_ */
