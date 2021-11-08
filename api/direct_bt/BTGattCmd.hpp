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
#include <string>
#include <mutex>
#include <condition_variable>

#include <jau/uuid.hpp>
#include <jau/octets.hpp>

#include "BTGattDesc.hpp"
#include "BTGattChar.hpp"
#include "BTGattService.hpp"
#include "HCITypes.hpp"

#ifndef BT_GATT_CMD_HPP_
#define BT_GATT_CMD_HPP_

namespace direct_bt {

    /**
     * Class maps a GATT command and optionally its asynchronous response
     * to a synchronous atomic operation.
     *
     * The GATT command is issued by writing the associated GATT characteristic value
     * via BTGattChar::writeValueNoResp() or BTGattChar::writeValue().
     *
     * Its optional asynchronous characteristic value notification or indication response
     * is awaited and collected after command issuance.
     *
     * If a response jau::uuid_t is given, notification or indication will be enabled at first send() command
     * and disabled at close() or destruction.
     *
     * @see BTGattChar::writeValueNoResp()
     * @see BTGattChar::writeValue()
     * @since 2.4.0
     */
    class BTGattCmd
    {
        private:
            /** Name, representing the command */
            std::string name;
            /** Command's BTGattService jau::uuid_t, may be nullptr. */
            const jau::uuid_t* service_uuid;
            /** Command's BTGattChar value jau::uuid_t to write command, never nullptr. */
            const jau::uuid_t* cmd_uuid;
            /** Command's optional BTGattChar value jau::uuid_t for the notification or indication response, may be nullptr. */
            const jau::uuid_t* rsp_uuid;

            std::mutex mtxCommand;
            std::mutex mtxRspReceived;
            std::condition_variable cvRspReceived;
            BTDevice& dev;
            jau::POctets rsp_data;
            BTGattCharRef cmdCharRef;
            BTGattCharRef rspCharRef;
            bool setup_done;

            class ResponseCharListener : public BTGattChar::Listener {
                private:
                    BTGattCmd& source;
                    jau::POctets& rsp_data;

                public:
                    ResponseCharListener(BTGattCmd& source_, jau::POctets& rsp_data_)
                    : source(source_), rsp_data(rsp_data_)
                    { }

                    void notificationReceived(BTGattCharRef charDecl,
                                              const jau::TROOctets& char_value, const uint64_t timestamp) override;

                    void indicationReceived(BTGattCharRef charDecl,
                                            const jau::TROOctets& char_value, const uint64_t timestamp,
                                            const bool confirmationSent) override;
            };
            std::shared_ptr<ResponseCharListener> rspCharListener;
            bool verbose;

            std::string rspCharStr() const noexcept {
                return nullptr != rspCharRef ? rspCharRef->toString() : "n/a";
            }
            std::string srvUUIDStr() const noexcept {
                return nullptr != service_uuid ? service_uuid->toString() : "n/a";
            }
            std::string rspUUIDStr() const noexcept {
                return nullptr != rsp_uuid ? rsp_uuid->toString() : "n/a";
            }
            bool isConnected() const noexcept;

            bool isResolvedEq() const noexcept {
                return nullptr != cmdCharRef && cmdCharRef->isValid();
            }

            HCIStatusCode setup() noexcept;

        public:
            /**
             * Constructor for commands with notification or indication response.
             *
             * @param dev_ the remote BTDevice
             * @param name_ user given name, representing the command
             * @param service_uuid_ command's BTGattService jau::uuid_t
             * @param cmd_uuid_ command's BTGattChar value jau::uuid_t to write the command
             * @param rsp_uuid_ command's BTGattChar value jau::uuid_t for the notification or indication response.
             * @param rsp_capacity initial capacity of caller owned response sink with sufficient capacity
             */
            BTGattCmd(BTDevice& dev_, const std::string& name_,
                      const jau::uuid_t& service_uuid_,
                      const jau::uuid_t& cmd_uuid_,
                      const jau::uuid_t& rsp_uuid_,
                      const jau::nsize_t rsp_capacity) noexcept
            : name(name_),
              service_uuid(&service_uuid_),
              cmd_uuid(&cmd_uuid_),
              rsp_uuid(&rsp_uuid_),
              dev(dev_),
              rsp_data(rsp_capacity, 0 /* size */, jau::endian::little),
              cmdCharRef(nullptr),
              rspCharRef(nullptr),
              setup_done(false),
              rspCharListener( std::shared_ptr<ResponseCharListener>( new ResponseCharListener(*this, rsp_data) ) ),
              verbose(jau::environment::get().debug)
            { }

            /**
             * Constructor for commands with notification or indication response.
             *
             * Since no service UUID is given, the BTGattChar lookup is less efficient.
             *
             * @param dev_ the remote BTDevice
             * @param name_ user given name, representing the command
             * @param cmd_uuid_ command's BTGattChar value jau::uuid_t to write the command
             * @param rsp_uuid_ command's BTGattChar value jau::uuid_t for the notification or indication response.
             * @param rsp_capacity initial capacity of caller owned response sink with sufficient capacity
             */
            BTGattCmd(BTDevice& dev_, const std::string& name_,
                      const jau::uuid_t& cmd_uuid_,
                      const jau::uuid_t& rsp_uuid_,
                      const jau::nsize_t rsp_capacity) noexcept
            : name(name_),
              service_uuid(nullptr),
              cmd_uuid(&cmd_uuid_),
              rsp_uuid(&rsp_uuid_),
              dev(dev_),
              rsp_data(rsp_capacity, 0 /* size */, jau::endian::little),
              cmdCharRef(nullptr),
              rspCharRef(nullptr),
              setup_done(false),
              rspCharListener( std::shared_ptr<ResponseCharListener>( new ResponseCharListener(*this, rsp_data) ) ),
              verbose(jau::environment::get().debug)
            { }

            /**
             * Constructor for commands without response.
             *
             * @param dev_ the remote BTDevice
             * @param name_ user given name, representing the command
             * @param service_uuid_ command's BTGattService jau::uuid_t
             * @param cmd_uuid_ command's BTGattChar value jau::uuid_t to write the command
             */
            BTGattCmd(BTDevice& dev_, const std::string& name_,
                      const jau::uuid_t& service_uuid_,
                      const jau::uuid_t& cmd_uuid_) noexcept
            : name(name_),
              service_uuid(&service_uuid_),
              cmd_uuid(&cmd_uuid_),
              rsp_uuid(nullptr),
              dev(dev_),
              rsp_data(jau::endian::little),
              cmdCharRef(nullptr),
              rspCharRef(nullptr),
              setup_done(false),
              rspCharListener( nullptr ),
              verbose(jau::environment::get().debug)
            { }

            /**
             * Constructor for commands without response.
             *
             * Since no service UUID is given, the BTGattChar lookup is less efficient.
             *
             * @param dev_ the remote BTDevice
             * @param name_ user given name, representing the command
             * @param cmd_uuid_ command's BTGattChar value jau::uuid_t to write the command
             */
            BTGattCmd(BTDevice& dev_, const std::string& name_,
                      const jau::uuid_t& cmd_uuid_) noexcept
            : name(name_),
              service_uuid(nullptr),
              cmd_uuid(&cmd_uuid_),
              rsp_uuid(nullptr),
              dev(dev_),
              rsp_data(jau::endian::little),
              cmdCharRef(nullptr),
              rspCharRef(nullptr),
              setup_done(false),
              rspCharListener( nullptr ),
              verbose(jau::environment::get().debug)
            { }

            /**
             * Close this command instance, usually called at destruction.
             *
             * If a response jau::uuid_t has been given, notification or indication will be disabled.
             */
            HCIStatusCode close() noexcept;

            ~BTGattCmd() noexcept { close(); }

            /** Return name, representing the command */
            const std::string& getName() const noexcept { return name; }

            /** Return command's BTGattService jau::uuid_t, may be nullptr. */
            const jau::uuid_t* getServiceUUID() const noexcept { return service_uuid; }

            /** Return command's BTGattChar value jau::uuid_t to write command, never nullptr. */
            const jau::uuid_t* getCommandUUID() const noexcept { return cmd_uuid; }

            /** Return true if a notification or indication response has been set via constructor, otherwise false. */
            bool hasResponseSet() const noexcept { return nullptr != rsp_uuid; }

            /** Return command's optional BTGattChar value jau::uuid_t for the notification or indication response, may be nullptr. */
            const jau::uuid_t* getResponseUUID() const noexcept { return rsp_uuid; }

            /** Set verbosity for UUID resolution. */
            void setVerbose(const bool v) noexcept { verbose = v; }

            /**
             * Returns the read-only response data object
             * for configured commands with response notification or indication.
             *
             * jau::TROOctets::size() matches the size of last received command response or zero.
             */
            const jau::TROOctets& getResponse() const noexcept { return rsp_data; }

            /**
             * Query whether all UUIDs of this commands have been resolved.
             *
             * In case no command has been issued via send() yet,
             * the UUIDs will be resolved with this call.
             *
             * @return true if all UUIDs have been resolved, otherwise false
             */
            bool isResolved() noexcept;

            /**
             * Send the command to the remote BTDevice.
             *
             * If a notification or indication result jau::uuid_t has been set via constructor,
             * it will be awaited and can be retrieved via getResponse() after command returns.
             *
             * @param prefNoAck pass true to prefer command write without acknowledge, otherwise use with-ack if available
             * @param cmd_data raw command octets
             * @param timeoutMS timeout in milliseconds. Defaults to 10 seconds limited blocking for the response to become available
             * @return
             */
            HCIStatusCode send(const bool prefNoAck, const jau::TROOctets& cmd_data, const int timeoutMS=10000) noexcept;

            std::string toString() const noexcept;
    };
}

#endif // BT_GATT_CMD_HPP_
