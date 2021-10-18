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

#ifndef ATT_PDU_TYPES_HPP_
#define ATT_PDU_TYPES_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>

#include <mutex>
#include <atomic>

#include <jau/basic_types.hpp>
#include <jau/octets.hpp>
#include <jau/uuid.hpp>

#include "BTTypes0.hpp"


/**
 * - - - - - - - - - - - - - - -
 * # Direct-BT Overview
 *
 * Direct-BT provides direct Bluetooth LE and BREDR programming,
 * offering robust high-performance support for embedded & desktop with zero overhead via C++ and Java.
 *
 * Direct-BT follows the official [Bluetooth Specification](https://www.bluetooth.com/specifications/bluetooth-core-specification/)
 * and its C++ implementation contains detailed references.
 *
 * Direct-BT supports a fully event driven workflow from adapter management via device discovery to GATT programming,
 * using its platform agnostic HCI, GATT, SMP and L2CAP client-side protocol implementation.
 *
 * - - - - - - - - - - - - - - -
 *
 * ## Direct-BT Layers
 *
 * - BTManager for adapter configuration and adapter add/removal notifications (ChangedAdapterSetFunc())
 *   - Using *BlueZ Kernel Manager Control Channel* via MgmtMsg communication.
 * - *HCI Handling* via HCIHandler using HCIPacket implementing connect/disconnect w/ tracking, device discovery, etc
 * - *ATT PDU* AttPDUMsg via L2CAP for low level packet communication
 * - *GATT Support* via BTGattHandler using AttPDUMsg over L2CAPComm, providing
 *   -  BTGattService
 *   -  BTGattChar
 *   -  BTGattDesc
 * - *SMP PDU* SMPPDUMsg via L2CAP for Security Manager Protocol (SMP) communication
 * - *SMP Support* via SMPHandler using SMPPDUMsg over L2CAPComm, providing (Not yet supported by Linux/BlueZ)
 *   - LE Secure Connections
 *   - LE legacy pairing
 * - On Linux/BlueZ, LE Secure Connections and LE legacy pairing is supported using
 *   - ::BTSecurityLevel setting via BTDevice / L2CAPComm per connection and
 *   - ::SMPIOCapability via BTManager (per adapter) and BTDevice (per connection)
 *   - SMPPDUMsg SMP event tracking over HCI/ACL/L2CAP, observing operations
 *
 * BTManager utilizes the *BlueZ Kernel Manager Control Channel*
 * for adapter configuration and adapter add/removal notifications (ChangedAdapterSetFunc()).
 *
 * To support other platforms than Linux/BlueZ, we will have to
 * - Move specified HCI host features used in BTManager to HCIHandler, SMPHandler,..  - and -
 * - Add specialization for each new platform using their non-platform-agnostic features.
 *
 * - - - - - - - - - - - - - - -
 *
 * ## Direct-BT User Hierarchy
 *
 * From a user perspective the following hierarchy is provided
 * - BTManager has zero or more
 *   - BTAdapter has zero or more
 *     - BTDevice has zero or more
 *       - BTGattService has zero or more
 *         - BTGattChar has zero or more
 *           - BTGattDesc
 *
 * - - - - - - - - - - - - - - -
 *
 * ## Direct-BT Object Lifecycle
 *
 * Object lifecycle with all instances and marked weak back-references to their owner
 * - BTManager singleton instance for all
 * - BTAdapter ownership by DBTManager
 *   - BTDevice ownership by DBTAdapter
 *     - BTGattHandler ownership by BTDevice, with weak BTDevice back-reference
 *       - BTGattService ownership by BTGattHandler, with weak BTGattHandler back-reference
 *         - BTGattChar ownership by BTGattService, with weak BTGattService back-reference
 *           - BTGattDesc ownership by BTGattChar, with weak BTGattChar back-reference
 *
 * - - - - - - - - - - - - - - -
 *
 * ## Direct-BT Mapped Names C++ vs Java
 *
 * Mapped names from C++ implementation to Java implementation and to Java interface:
 *
 *  C++  <br> `direct_bt` | Java Implementation  <br> `jau.direct_bt` | Java Interface <br> `org.direct_bt` |
 *  :----------------| :---------------------| :--------------------|
 *  BTManager        | DBTManager            | BTManager            |
 *  BTAdapter        | DBTAdapter            | BTAdapter            |
 *  BTDevice         | DBTDevice             | BTDevice             |
 *  BTGattService    | DBTGattService        | BTGattService        |
 *  BTGattChar       | DBTGattChar           | BTGattChar           |
 *  BTGattDesc       | DBTGattDesc           | BTGattDesc           |
 *  AdapterStatusListener |                  | AdapterStatusListener   |
 *  BTGattCharListener |                     | BTGattCharListener   |
 *  ChangedAdapterSetFunc() |                | BTManager::ChangedAdapterSetListener   |
 *
 * - - - - - - - - - - - - - - -
 *
 * ## Direct-BT Event Driven Workflow
 *
 * A fully event driven workflow from adapter management via device discovery to GATT programming is supported.
 *
 * ChangedAdapterSetFunc() allows listening to added and removed BTAdapter via BTManager.
 *
 * AdapterStatusListener allows listening to BTAdapter changes and BTDevice discovery.
 *
 * BTGattCharListener allows listening to GATT indications and notifications.
 *
 * Main event listener can be attached to these objects
 * which maintain a set of unique listener instances without duplicates.
 *
 * - BTManager
 *   - ChangedAdapterSetFunc()
 *
 * - BTAdapter
 *   - AdapterStatusListener
 *
 * - BTGattChar or BTGattHandler
 *   - BTGattCharListener
 *
 * Other API attachment method exists for BTGattCharListener,
 * however, they only exists for convenience and end up to be attached to BTGattHandler.
 *
 *
 * - - - - - - - - - - - - - - -
 *
 * BT Core Spec v5.2:  Vol 3, Part A L2CAP Spec: 7.9 PRIORITIZING DATA OVER HCI
 *
 * > In order for guaranteed channels to meet their guarantees,
 * > L2CAP should prioritize traffic over the HCI transport in devices that support HCI.
 * > Packets for Guaranteed channels should receive higher priority than packets for Best Effort channels.
 *
 * - - - - - - - - - - - - - - -
 *
 * ATTPDUTypes.hpp Module for ATTPDUMsg Types:
 *
 * - BT Core Spec v5.2: Vol 3, Part F Attribute Protocol (ATT)
 */
namespace direct_bt {

    class AttException : public jau::RuntimeException {
        protected:
            AttException(std::string const type, std::string const m, const char* file, int line) noexcept
            : RuntimeException(type, m, file, line) {}

        public:
            AttException(std::string const m, const char* file, int line) noexcept
            : RuntimeException("AttException", m, file, line) {}
    };

    class AttOpcodeException : public AttException {
        public:
            AttOpcodeException(std::string const m, const char* file, int line) noexcept
            : AttException("AttOpcodeException", m, file, line) {}
    };

    class AttValueException : public AttException {
        public:
            AttValueException(std::string const m, const char* file, int line) noexcept
            : AttException("AttValueException", m, file, line) {}
    };

    /**
     * Handles the Attribute Protocol (ATT) using Protocol Data Unit (PDU)
     * encoded messages over L2CAP channel.
     * <p>
     * Implementation uses persistent memory w/ ownership
     * copying PDU data to allow intermediate pipe processing.
     * </p>
     * <p>
     *
     * Vol 3, Part F 2 - Protocol Overview pp
     * ---------------------------------------
     * One attribute := { UUID type; uint16_t handle; permissions for higher layer; },
     * where
     *
     * UUID is an official assigned number,
     *
     * handle uniquely references an attribute on a server for client R/W access,
     * see Vol 3, Part F 3.4.4 - 3.4.6, also 3.4.7 (notified/indicated),
     * 3.4.3 (discovery) and 3.2.5 (permissions).
     *
     * Client sends ATT requests to a server, which shall respond to all.
     *
     * A device can take client and server roles concurrently.
     *
     * One server per device, ATT handle is unique for all supported bearers.
     * For each client, server has one set of ATTs.
     * The server (and hence device) can support multiple clients.
     *
     * Services are distinguished by range of handles for each service.
     * Discovery is of these handle ranges is defined by a higher layer spec.
     *
     * ATT Protocol has notification and indication capabilities for efficient
     * ATT value promotion to client w/o reading them (Vol 3, Part F 3.3).
     *
     * All ATT Protocol requests sent over an ATT bearer.
     * Multiple ATT bearers can be established between two devices.
     * Each ATT bearer uses a separate L2CAP channel an can have different configurations.
     *
     * For LE a single ATT bearer using a fixed L2CAP channel is available ASAP after
     * ACL connection is established.
     * Additional ATT bearers can be established using L2CAP (Vol 3, Part F 3.2.11).
     * </p>
     *
     * <p>
     * Vol 3, Part F 3 - Basics and Types
     * ------------------------------------
     * ATT handle is uint16_t and valid if > 0x0000, max is 0xffff.
     * ATT handle is unique per server.
     *
     * ATT value (Vol 3, Part F 3.2.4)
     *
     * - ATT value is uint8_t array of fixed or variable length.
     *
     * - ATT values might be too large for a single PDU,
     *   hence it must be sent using multiple PDUs.
     *
     * - ATT value encoding is defined by the ATT type (UUID).
     *
     * - ATT value transmission done via request, response,
     *   notification or indication
     *
     * - ATT value variable length is implicit by PDU carrying packet (PDU parent),
     *   implying:
     *   - One ATT value per ATT request... unless ATT values have fixed length.
     *   - Only one ATT value with variable length in a request...
     *   - L2CAP preserves DGRAM boundaries
     *
     *   Some PDUs include the ATT value length, for which above limitations don't apply.
     *
     *   Maximum length of an attribute value shall be 512 bytes (Vol 3, Part F 3.2.8),
     *   spread across multiple PDUs.
     * </p>
     *
     * - BT Core Spec v5.2: Vol 3, Part A: BT Logical Link Control and Adaption Protocol (L2CAP)
     *
     * - BT Core Spec v5.2: Vol 3, Part F Attribute Protocol (ATT)
     *
     * - BT Core Spec v5.2: Vol 3, Part F 3 ATT PDUs (Protocol Data Unit)
     *
     * - BT Core Spec v5.2: Vol 3, Part F 3.3 ATT PDUs
     *
     * - BT Core Spec v5.2: Vol 3, Part F 4 Security Considerations
     *
     * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     *
     * AttPDUMsg Base Class
     * =====================
     * Attribute Protocol (ATT)'s Protocol Data Unit (PDU) message
     * Vol 3, Part F 3.3 and Vol 3, Part F 3.4
     *
     * Little endian, however, ATT value endianess is defined by above layer.
     *
     * ATT_MTU Specification
     * - BT Core Spec v5.2: Vol 3, Part F ATT: 3.2.8 Exchanging MTU size
     * - BT Core Spec v5.2: Vol 3, Part F ATT: 3.2.9 Long attribute values
     * - BT Core Spec v5.2: Vol 3, Part G GATT: 5.2.1 ATT_MTU
     *
     * resulting in a ATT_MTU range of
     *
     * - ATT_MTU minimum is 23 bytes (Vol 3, Part G: 5.2.1)
     * - ATT_MTU is negotiated, maximum is 512 bytes (Vol 3, Part F: 3.2.8-9)
     * - ATT Value sent: [1 .. ATT_MTU-1] (Vol 3, Part F: 3.2.8-9)
     *
     *
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.3.1 Attribute PDU Format
     * -----------------------------------------------------------------
     * ```
     *   { uint8_t opcode, uint8_t param[0..ATT_MTU-X], uint8_t auth_sig[0||12] }
     * ```
     * with
     * ```
     *   opcode bits{ 0-5 method, 6 command-flag, 7 auth-sig-flag }
     * ```
     * and
     * ```
     *   X =  1 if auth-sig flag of ATT-opcode is 0, or
     *   X = 13 if auth-sig flag of ATT-opcode is 1.
     * ```
     * </p>
     */
    class AttPDUMsg
    {
        public:
            /** ATT Opcode Summary Vol 3, Part F 3.4.8 */
            enum class Opcode : uint8_t {
                PDU_UNDEFINED               = 0x00, // our own pseudo opcode, indicating no ATT PDU message

                METHOD_MASK                 = 0x3F, // bits 0 .. 5
                COMMAND_FLAG                = 0x40, // bit 6 (counting from 0)
                AUTH_SIGNATURE_FLAG         = 0x80, // bit 7 (counting from 0)

                ERROR_RSP                   = 0x01,
                EXCHANGE_MTU_REQ            = 0x02,
                EXCHANGE_MTU_RSP            = 0x03,
                FIND_INFORMATION_REQ        = 0x04,
                FIND_INFORMATION_RSP        = 0x05,
                FIND_BY_TYPE_VALUE_REQ      = 0x06,
                FIND_BY_TYPE_VALUE_RSP      = 0x07,
                READ_BY_TYPE_REQ            = 0x08,
                READ_BY_TYPE_RSP            = 0x09,
                READ_REQ                    = 0x0A,
                READ_RSP                    = 0x0B,
                READ_BLOB_REQ               = 0x0C,
                READ_BLOB_RSP               = 0x0D,
                READ_MULTIPLE_REQ           = 0x0E,
                READ_MULTIPLE_RSP           = 0x0F,
                READ_BY_GROUP_TYPE_REQ      = 0x10,
                READ_BY_GROUP_TYPE_RSP      = 0x11,
                WRITE_REQ                   = 0x12,
                WRITE_RSP                   = 0x13,
                WRITE_CMD                   = WRITE_REQ + COMMAND_FLAG, // = 0x52
                PREPARE_WRITE_REQ           = 0x16,
                PREPARE_WRITE_RSP           = 0x17,
                EXECUTE_WRITE_REQ           = 0x18,
                EXECUTE_WRITE_RSP           = 0x19,

                READ_MULTIPLE_VARIABLE_REQ  = 0x20,
                READ_MULTIPLE_VARIABLE_RSP  = 0x21,

                MULTIPLE_HANDLE_VALUE_NTF   = 0x23,

                HANDLE_VALUE_NTF            = 0x1B,
                HANDLE_VALUE_IND            = 0x1D,
                HANDLE_VALUE_CFM            = 0x1E,

                SIGNED_WRITE_CMD            = WRITE_REQ + COMMAND_FLAG + AUTH_SIGNATURE_FLAG // = 0xD2
            };
            static constexpr uint8_t number(const Opcode rhs) noexcept {
                return static_cast<uint8_t>(rhs);
            }
            static std::string getOpcodeString(const Opcode opc) noexcept;

            enum class ReqRespType : bool {
                REQUEST = true,
                RESPONSE = false
            };
            static constexpr bool is_request(const ReqRespType rhs) noexcept {
                return static_cast<bool>(rhs);
            }

            enum class OpcodeType : uint8_t {
                UNDEFINED     = 0,
                REQUEST       = 1,
                RESPONSE      = 2,
                NOTIFICATION  = 3,
                INDICATION    = 4
            };

            static constexpr OpcodeType get_type(const Opcode rhs) noexcept {
                switch(rhs) {
                    case Opcode::MULTIPLE_HANDLE_VALUE_NTF:
                        [[fallthrough]];
                    case Opcode::HANDLE_VALUE_NTF:
                        return OpcodeType::NOTIFICATION;

                    case Opcode::HANDLE_VALUE_IND:
                        return OpcodeType::INDICATION;

                    case Opcode::ERROR_RSP:
                        [[fallthrough]];
                    case Opcode::EXCHANGE_MTU_RSP:
                        [[fallthrough]];
                    case Opcode::FIND_INFORMATION_RSP:
                        [[fallthrough]];
                    case Opcode::FIND_BY_TYPE_VALUE_RSP:
                        [[fallthrough]];
                    case Opcode::READ_BY_TYPE_RSP:
                        [[fallthrough]];
                    case Opcode::READ_RSP:
                        [[fallthrough]];
                    case Opcode::READ_BLOB_RSP:
                        [[fallthrough]];
                    case Opcode::READ_MULTIPLE_RSP:
                        [[fallthrough]];
                    case Opcode::READ_BY_GROUP_TYPE_RSP:
                        [[fallthrough]];
                    case Opcode::WRITE_RSP:
                        [[fallthrough]];
                    case Opcode::PREPARE_WRITE_RSP:
                        [[fallthrough]];
                    case Opcode::EXECUTE_WRITE_RSP:
                        [[fallthrough]];
                    case Opcode::READ_MULTIPLE_VARIABLE_RSP:
                        return OpcodeType::RESPONSE;

                    case Opcode::EXCHANGE_MTU_REQ:
                        [[fallthrough]];
                    case Opcode::FIND_INFORMATION_REQ:
                        [[fallthrough]];
                    case Opcode::FIND_BY_TYPE_VALUE_REQ:
                        [[fallthrough]];
                    case Opcode::READ_BY_TYPE_REQ:
                        [[fallthrough]];
                    case Opcode::READ_REQ:
                        [[fallthrough]];
                    case Opcode::READ_BLOB_REQ:
                        [[fallthrough]];
                    case Opcode::READ_MULTIPLE_REQ:
                        [[fallthrough]];
                    case Opcode::READ_BY_GROUP_TYPE_REQ:
                        [[fallthrough]];
                    case Opcode::WRITE_REQ:
                        [[fallthrough]];
                    case Opcode::WRITE_CMD:
                        [[fallthrough]];
                    case Opcode::PREPARE_WRITE_REQ:
                        [[fallthrough]];
                    case Opcode::EXECUTE_WRITE_REQ:
                        [[fallthrough]];
                    case Opcode::READ_MULTIPLE_VARIABLE_REQ:
                        [[fallthrough]];
                    case Opcode::SIGNED_WRITE_CMD:
                        [[fallthrough]];
                    case Opcode::HANDLE_VALUE_CFM: // A response from master/gatt-client to slave/gatt-server
                        return OpcodeType::REQUEST;

                    default:
                        return OpcodeType::UNDEFINED;
                }
            }

        private:
            static constexpr Opcode bit_and(const Opcode lhs, const Opcode rhs) noexcept {
                return static_cast<Opcode> ( static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs) );
            }
            static constexpr bool bit_test(const Opcode lhs, const Opcode rhs) noexcept {
                return 0 != ( static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs) );
            }

        protected:
            void checkOpcode(const Opcode expected) const
            {
                const Opcode has = getOpcode();
                if( expected != has ) {
                    throw AttOpcodeException("Has opcode "+jau::to_hexstring(number(has))+" "+getOpcodeString(has)+
                                     ", but expected "+jau::to_hexstring(number(expected))+" "+getOpcodeString(expected), E_FILE_LINE);
                }
            }
            void checkOpcode(const Opcode exp1, const Opcode exp2) const
            {
                const Opcode has = getOpcode();
                if( exp1 != has && exp2 != has ) {
                    throw AttOpcodeException("Has opcode "+jau::to_hexstring(number(has))+" "+getOpcodeString(has)+
                                     ", but expected either "+jau::to_hexstring(number(exp1))+" "+getOpcodeString(exp1)+
                                     " or  "+jau::to_hexstring(number(exp1))+" "+getOpcodeString(exp1), E_FILE_LINE);
                }
            }

            virtual std::string baseString() const noexcept {
                return "opcode="+jau::to_hexstring(number(getOpcode()))+" "+getOpcodeString(getOpcode())+
                        ", size[total="+std::to_string(pdu.size())+", param "+std::to_string(getPDUParamSize())+"]";
            }
            virtual std::string valueString() const noexcept {
                return "size "+std::to_string(getPDUValueSize())+", data "
                        +jau::bytesHexString(pdu.get_ptr(), getPDUValueOffset(), getPDUValueSize(), true /* lsbFirst */);
            }

        public:
            /** actual received PDU */
            jau::POctets pdu;

            /** creation timestamp in milliseconds */
            const uint64_t ts_creation;

            /**
             * Return a newly created specialized instance pointer to base class.
             * <p>
             * Returned memory reference is managed by caller (delete etc)
             * </p>
             */
            static std::unique_ptr<const AttPDUMsg> getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept;

            /** Persistent memory, w/ ownership ..*/
            AttPDUMsg(const uint8_t* source, const jau::nsize_t size)
                : pdu(source, std::max<jau::nsize_t>(1, size), jau::endian::little),
                  ts_creation(jau::getCurrentMilliseconds())
            {
                pdu.check_range(0, getPDUMinSize());
            }

            /** Persistent memory, w/ ownership ..*/
            AttPDUMsg(const Opcode opc, const jau::nsize_t size)
                : pdu(std::max<jau::nsize_t>(1, size), jau::endian::little),
                  ts_creation(jau::getCurrentMilliseconds())
            {
                pdu.put_uint8_nc(0, number(opc));
                pdu.check_range(0, getPDUMinSize());
            }

            AttPDUMsg(const AttPDUMsg &o) noexcept = default;
            AttPDUMsg(AttPDUMsg &&o) noexcept = default;
            AttPDUMsg& operator=(const AttPDUMsg &o) noexcept = delete; // const ts_creation
            AttPDUMsg& operator=(AttPDUMsg &&o) noexcept = delete; // const ts_creation

            virtual ~AttPDUMsg() noexcept {}

            /** ATT PDU Format Vol 3, Part F 3.3.1 */
            constexpr Opcode getOpcode() const noexcept { return static_cast<Opcode>(pdu.get_uint8_nc(0)); }

            /** ATT PDU Format Vol 3, Part F 3.3.1 */
            constexpr Opcode getOpMethod() const noexcept { return bit_and(getOpcode(), Opcode::METHOD_MASK); }

            /** ATT PDU Format Vol 3, Part F 3.3.1 */
            constexpr bool getOpCommandFlag() const noexcept { return bit_test(getOpcode(), Opcode::COMMAND_FLAG); }

            /** ATT PDU Format Vol 3, Part F 3.3.1 */
            constexpr bool getOpAuthSigFlag() const noexcept { return bit_test(getOpcode(), Opcode::AUTH_SIGNATURE_FLAG); }

            /**
             * ATT PDU Format Vol 3, Part F 3.3.1
             * <p>
             * The ATT Authentication Signature size in octets.
             * </p>
             * <p>
             * This auth-signature comes at the very last of the PDU.
             * </p>
             */
            constexpr jau::nsize_t getAuthSigSize() const noexcept { return getOpAuthSigFlag() ? 12 : 0; }

            /**
             * ATT PDU Format Vol 3, Part F 3.3.1
             * <p>
             * The ATT PDU parameter size in octets
             * less opcode (1 byte) and auth-signature (0 or 12 bytes).
             * </p>
             * <pre>
             *   param-size := pdu.size - getAuthSigSize() - 1
             * </pre>
             * <p>
             * Note that the PDU parameter include the PDU value below.
             * </p>
             * <p>
             * Note that the optional auth-signature is at the end of the PDU.
             * </p>
             */
            constexpr jau::nsize_t getPDUParamSize() const noexcept {
                return pdu.size() - getAuthSigSize() - 1 /* opcode */;
            }

            /**
             * Returns the octet offset to the value segment in this PDU
             * including the mandatory opcode,
             * i.e. the number of octets until the first value octet.
             * <p>
             * Note that the ATT PDU value is part of the PDU param,
             * where it is the last segment.
             * </p>
             * <p>
             * The value offset is ATT PDU specific and may point
             * to the variable user data post handle etc within the PDU Param block.
             * </p>
             * <p>
             * Note that the opcode must be included in the implementation,
             * as it may be used to reference the value in the pdu
             * conveniently.
             * </p>
             */
            constexpr_cxx20 virtual jau::nsize_t getPDUValueOffset() const noexcept { return 1; /* default: opcode */ }

            /**
             * Returns this PDU's minimum size, i.e.
             * <pre>
             *   opcode + param - value + auth_signature
             * </pre>
             * Value is excluded as it might be flexible.
             */
            constexpr_cxx20 jau::nsize_t getPDUMinSize() const noexcept {
                return getPDUValueOffset() + getAuthSigSize();
            }

            /**
             * Returns the net octet size of this PDU's attributes value,
             * i.e.
             * - `pdu.size - getAuthSigSize() - value-offset` or
             * - `getPDUParamSize() - getPDUValueOffset() + 1`
             * <p>
             * Note that the opcode size of 1 octet is re-added as included in getPDUValueOffset()
             * for convenience but already taken-off in getPDUParamSize() for spec compliance!
             * </p>
             * <pre>
             *   value-size := param-size - value-offset + 1
             *   param-size := pdu.size - getAuthSigSize() - 1
             *
             *   value-size := pdu.size - getAuthSigSize() - 1 - value-offset + 1
             *   value-size := pdu.size - getAuthSigSize() - value-offset
             * </pre>
             */
            constexpr_cxx20 jau::nsize_t getPDUValueSize() const noexcept { return getPDUParamSize() - getPDUValueOffset() + 1; }

            /**
             * Returns the theoretical maximum value size of a PDU's attribute value.
             * <pre>
             *  ATT_MTU - getAuthSigSize() - value-offset
             * </pre>
             */
            constexpr_cxx20 jau::nsize_t getMaxPDUValueSize(const jau::nsize_t mtu) const noexcept {
                return mtu - getAuthSigSize() - getPDUValueOffset();
            }

            constexpr_cxx20 virtual std::string getName() const noexcept {
                return "AttPDUMsg";
            }

            virtual std::string toString() const noexcept{
                return getName()+"["+baseString()+", value["+valueString()+"]]";
            }
    };

    /**
     * Our own pseudo opcode, indicating no ATT PDU message.
     * <p>
     * ATT_PDU_UNDEFINED
     * </p>
     */
    class AttPDUUndefined: public AttPDUMsg
    {
        public:
            AttPDUUndefined(const uint8_t* source, const jau::nsize_t length) : AttPDUMsg(source, length) {
                checkOpcode(Opcode::PDU_UNDEFINED);
            }

            constexpr_cxx20 jau::nsize_t getPDUValueOffset() const noexcept override { return 1; }

            constexpr_cxx20 std::string getName() const noexcept override {
                return "AttPDUUndefined";
            }
    };

    /**
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.1.1 ATT_ERROR_RSP
     *
     * Used to send a error reply for any request.
     */
    class AttErrorRsp: public AttPDUMsg
    {
        public:
            enum class ErrorCode : uint8_t {
                /** Direct-BT's extension to indicate no error */
                NO_ERROR                    = 0x00,
                INVALID_HANDLE              = 0x01,
                NO_READ_PERM                = 0x02,
                NO_WRITE_PERM               = 0x03,
                INVALID_PDU                 = 0x04,
                INSUFF_AUTHENTICATION       = 0x05,
                UNSUPPORTED_REQUEST         = 0x06,
                INVALID_OFFSET              = 0x07,
                INSUFF_AUTHORIZATION        = 0x08,
                PREPARE_QUEUE_FULL          = 0x09,
                ATTRIBUTE_NOT_FOUND         = 0x0A,
                ATTRIBUTE_NOT_LONG          = 0x0B,
                INSUFF_ENCRYPTION_KEY_SIZE  = 0x0C,
                INVALID_ATTRIBUTE_VALUE_LEN = 0x0D,
                UNLIKELY_ERROR              = 0x0E,
                INSUFF_ENCRYPTION           = 0x0F,
                UNSUPPORTED_GROUP_TYPE      = 0x10,
                INSUFFICIENT_RESOURCES      = 0x11,
                DB_OUT_OF_SYNC              = 0x12,
                FORBIDDEN_VALUE             = 0x13
            };
            static constexpr uint8_t number(const ErrorCode rhs) noexcept {
                return static_cast<uint8_t>(rhs);
            }
            static std::string getErrorCodeString(const ErrorCode errorCode) noexcept;

            AttErrorRsp(const uint8_t* source, const jau::nsize_t length) : AttPDUMsg(source, length) {
                checkOpcode(Opcode::ERROR_RSP);
            }

            AttErrorRsp(const ErrorCode error_code, const Opcode cause_opc, const uint16_t cause_handle)
            : AttPDUMsg(Opcode::ERROR_RSP, 1+1+2+1)
            {
                pdu.put_uint8_nc(1, AttPDUMsg::number(cause_opc));
                pdu.put_uint16_nc(2, cause_handle);
                pdu.put_uint8_nc(4, number(error_code));
            }


            /** opcode + reqOpcodeCause + handleCause + errorCode */
            constexpr_cxx20 jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 1 + 2 + 1; }

            constexpr Opcode getCausingOpcode() const noexcept { return static_cast<Opcode>( pdu.get_uint8_nc(1) ); }

            constexpr uint16_t getCausingHandle() const noexcept { return pdu.get_uint16_nc(2); }

            constexpr ErrorCode getErrorCode() const noexcept { return static_cast<ErrorCode>(pdu.get_uint8_nc(4)); }

            constexpr_cxx20 std::string getName() const noexcept override {
                return "AttErrorRsp";
            }

        protected:
            std::string valueString() const noexcept override {
                const Opcode opc = getCausingOpcode();
                const ErrorCode ec = getErrorCode();
                return "error "+jau::to_hexstring(number(ec)) + ": " + getErrorCodeString(ec)+
                       ", cause(opc "+jau::to_hexstring(AttPDUMsg::number(opc))+": "+getOpcodeString(opc)+
                       ", handle "+jau::to_hexstring(getCausingHandle())+")";
            }
    };

    /**
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.2.1 ATT_EXCHANGE_MTU_REQ
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.2.2 ATT_EXCHANGE_MTU_RSP
     *
     * Used for
     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.3.1 Exchange MTU (Server configuration)
     */
    class AttExchangeMTU: public AttPDUMsg
    {
        public:
            AttExchangeMTU(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length)
            {
                checkOpcode(Opcode::EXCHANGE_MTU_RSP, Opcode::EXCHANGE_MTU_REQ);
            }

            AttExchangeMTU(const ReqRespType type, const uint16_t mtuSize)
            : AttPDUMsg(is_request(type) ? Opcode::EXCHANGE_MTU_REQ : Opcode::EXCHANGE_MTU_RSP, 1+2)
            {
                pdu.put_uint16_nc(1, mtuSize);
            }

            /** opcode + mtu-size */
            constexpr_cxx20 jau::nsize_t getPDUValueOffset() const noexcept override { return 1+2; }

            constexpr uint16_t getMTUSize() const noexcept { return pdu.get_uint16_nc(1); }

            constexpr_cxx20 std::string getName() const noexcept override {
                return "AttExchangeMTU";
            }

        protected:
            std::string valueString() const noexcept override {
                return "mtu "+std::to_string(getMTUSize());
            }
    };

    /**
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.3 ATT_READ_REQ
     *
     * Used for
     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.1 Read Characteristic Value
     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.12.1 Read Characteristic Descriptors
     */
    class AttReadReq : public AttPDUMsg
    {
        public:
            AttReadReq(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length)
            {
                checkOpcode(Opcode::READ_REQ);
            }

            AttReadReq(const uint16_t handle)
            : AttPDUMsg(Opcode::READ_REQ, getPDUValueOffset())
            {
                pdu.put_uint16_nc(1, handle);
            }

            /** opcode + handle */
            constexpr_cxx20 jau::nsize_t getPDUValueOffset() const noexcept override { return 1+2; }

            constexpr uint16_t getHandle() const noexcept { return pdu.get_uint16_nc( 1 ); }

            constexpr_cxx20 std::string getName() const noexcept override {
                return "AttReadReq";
            }

        protected:
            std::string valueString() const noexcept override {
                return "handle "+jau::to_hexstring(getHandle());
            }
    };

    /**
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.5 ATT_BLOB_READ_REQ
     *
     * Used for
     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.3 Read Long Characteristic Value
     * - For any follow up request, which previous request reply couldn't fit in ATT_MTU
     */
    class AttReadBlobReq : public AttPDUMsg
    {
        public:
            AttReadBlobReq(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length)
            {
                checkOpcode(Opcode::READ_BLOB_REQ);
            }

            AttReadBlobReq(const uint16_t handle, const uint16_t value_offset)
            : AttPDUMsg(Opcode::READ_BLOB_REQ, getPDUValueOffset())
            {
                pdu.put_uint16_nc(1, handle);
                pdu.put_uint16_nc(3, value_offset);
            }

            /** opcode + handle + value_offset */
            constexpr_cxx20 jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 2 + 2; }

            constexpr uint16_t getHandle() const noexcept { return pdu.get_uint16_nc( 1 ); }

            constexpr uint16_t getValueOffset() const noexcept { return pdu.get_uint16_nc( 1 + 2 ); }

            constexpr_cxx20 std::string getName() const noexcept override {
                return "AttReadBlobReq";
            }

        protected:
            std::string valueString() const noexcept override {
                return "handle "+jau::to_hexstring(getHandle())+", valueOffset "+jau::to_hexstring(getValueOffset());
            }
    };

    /**
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.4 ATT_READ_RSP
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.6 ATT_READ_BLOB_RSP
     *
     * Used for
     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.1 Read Characteristic Value
     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.12.1 Read Characteristic Descriptors
     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.3 Read Long Characteristic Value (Blob)
     *
     * Note:
     * ------
     * If expected value size exceeds getValueSize(), continue with ATT_READ_BLOB_REQ (3.4.4.5),
     * see shallReadBlobReq(..)
     */
    class AttReadNRsp: public AttPDUMsg
    {
        private:
            const jau::TOctetSlice view;

            constexpr static jau::nsize_t pdu_value_offset = 1;

        public:
            AttReadNRsp(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length),
              view(pdu, getPDUValueOffset(), getPDUValueSize())
            {
                checkOpcode(Opcode::READ_RSP, Opcode::READ_BLOB_RSP);
            }

            AttReadNRsp(const bool blobRsp, const jau::TROOctets & value, jau::nsize_t value_offset=0)
            : AttPDUMsg(blobRsp ? Opcode::READ_BLOB_RSP : Opcode::READ_RSP, getPDUValueOffset()+value.size()-value_offset),
              view(pdu, getPDUValueOffset(), getPDUValueSize())
            {
                pdu.put_bytes_nc(getPDUValueOffset(), value.get_ptr()+value_offset, value.size()-value_offset);
            }

            /** opcode */
            constexpr_cxx20 jau::nsize_t getPDUValueOffset() const noexcept override { return pdu_value_offset; }

            constexpr uint8_t const * getValuePtr() const noexcept { return pdu.get_ptr_nc( pdu_value_offset ); }

            constexpr jau::TOctetSlice const & getValue() const noexcept { return view; }

            constexpr_cxx20 std::string getName() const noexcept override {
                return "AttReadNRsp";
            }

        protected:
            std::string valueString() const noexcept override {
                return "size "+std::to_string(getPDUValueSize())+", data "+view.toString();
            }
    };

    /**
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.5.1 ATT_WRITE_REQ
     *
     * Reply
     * - ATT_WRITE_RSP -> AttWriteRsp
     *
     * Used for:
     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
     * - BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
     */
    class AttWriteReq : public AttPDUMsg
    {
        private:
            const jau::TOctetSlice view;

            constexpr static jau::nsize_t pdu_value_offset = 1 + 2;

        public:
            AttWriteReq(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length),
              view(pdu, getPDUValueOffset(), getPDUValueSize())
            {
                checkOpcode(Opcode::WRITE_REQ);
            }

            AttWriteReq(const uint16_t handle, const jau::TROOctets & value)
            : AttPDUMsg(Opcode::WRITE_REQ, getPDUValueOffset()+value.size()),
              view(pdu, getPDUValueOffset(), getPDUValueSize())
            {
                pdu.put_uint16_nc(1, handle);
                pdu.put_bytes_nc(getPDUValueOffset(), value.get_ptr(), value.size());
            }

            /** opcode + handle */
            constexpr_cxx20 jau::nsize_t getPDUValueOffset() const noexcept override { return pdu_value_offset; }

            constexpr uint16_t getHandle() const noexcept { return pdu.get_uint16_nc( 1 ); }

            constexpr uint8_t const * getValuePtr() const noexcept { return pdu.get_ptr_nc( pdu_value_offset ); }

            constexpr jau::TOctetSlice const & getValue() const noexcept { return view; }

            constexpr_cxx20 std::string getName() const noexcept override {
                return "AttWriteReq";
            }

        protected:
            std::string valueString() const noexcept override {
                return "handle "+jau::to_hexstring(getHandle())+", data "+view.toString();;
            }
    };

    /**
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.5.2 ATT_WRITE_RSP
     *
     * Used for:
     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
     */
    class AttWriteRsp : public AttPDUMsg
    {
        public:
            AttWriteRsp(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length) {
                checkOpcode(Opcode::WRITE_RSP);
            }

            AttWriteRsp()
            : AttPDUMsg(Opcode::WRITE_RSP, 1)
            { }

            /** opcode */
            constexpr_cxx20 jau::nsize_t getPDUValueOffset() const noexcept override { return 1; }

            constexpr_cxx20 std::string getName() const noexcept override {
                return "AttWriteRsp";
            }
    };

    /**
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.5.3 ATT_WRITE_CMD
     *
     * Reply
     * - None
     *
     * Used for:
     * - BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.1 Write Characteristic Value without Response
     */
    class AttWriteCmd : public AttPDUMsg
    {
        private:
            const jau::TOctetSlice view;

            constexpr static jau::nsize_t pdu_value_offset = 1 + 2;

        public:
            AttWriteCmd(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length),
              view(pdu, getPDUValueOffset(), getPDUValueSize())
            {
                checkOpcode(Opcode::WRITE_CMD);
            }

            AttWriteCmd(const uint16_t handle, const jau::TROOctets & value)
            : AttPDUMsg(Opcode::WRITE_CMD, getPDUValueOffset()+value.size()),
              view(pdu, getPDUValueOffset(), getPDUValueSize())
            {
                pdu.put_uint16_nc(1, handle);
                pdu.put_bytes_nc(getPDUValueOffset(), value.get_ptr(), value.size());
            }

            /** opcode + handle */
            constexpr_cxx20 jau::nsize_t getPDUValueOffset() const noexcept override { return pdu_value_offset; }

            constexpr uint16_t getHandle() const noexcept { return pdu.get_uint16_nc( 1 ); }

            constexpr uint8_t const * getValuePtr() const noexcept { return pdu.get_ptr_nc( pdu_value_offset ); }

            constexpr jau::TOctetSlice const & getValue() const noexcept { return view; }

            constexpr_cxx20 std::string getName() const noexcept override {
                return "AttWriteCmd";
            }

        protected:
            std::string valueString() const noexcept override {
                return "handle "+jau::to_hexstring(getHandle())+", data "+view.toString();;
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.7.1 and 3.4.7.2
     * <p>
     * A received ATT_HANDLE_VALUE_NTF or ATT_HANDLE_VALUE_IND from server.
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.10 Characteristic Value Notification
     * </p>
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.11 Characteristic Value Indications
     * </p>
     * <p>
     * Send by server to notify or indicate an ATT value (at any time).
     * </p>
     */
    class AttHandleValueRcv: public AttPDUMsg
    {
        private:
            const jau::TOctetSlice view;

            constexpr static jau::nsize_t pdu_value_offset = 1 + 2;

        public:
            AttHandleValueRcv(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length),
              view(pdu, getPDUValueOffset(), getPDUValueSize())
            {
                checkOpcode(Opcode::HANDLE_VALUE_NTF, Opcode::HANDLE_VALUE_IND);
            }

            AttHandleValueRcv(const bool isNotify, const uint16_t handle, const jau::TROOctets & value)
            : AttPDUMsg(isNotify ? Opcode::HANDLE_VALUE_NTF : Opcode::HANDLE_VALUE_IND, getPDUValueOffset()+value.size()),
              view(pdu, getPDUValueOffset(), getPDUValueSize())
            {
                pdu.put_uint16_nc(1, handle);
                pdu.put_bytes_nc(getPDUValueOffset(), value.get_ptr(), value.size());
            }

            /** opcode + handle */
            constexpr_cxx20 jau::nsize_t getPDUValueOffset() const noexcept override { return pdu_value_offset; }

            constexpr uint16_t getHandle() const noexcept { return pdu.get_uint16_nc(1); }

            constexpr uint8_t const * getValuePtr() const noexcept { return pdu.get_ptr_nc( pdu_value_offset ); }

            jau::TOctetSlice const & getValue() const noexcept { return view; }

            bool isNotification() const noexcept {
                return Opcode::HANDLE_VALUE_NTF == getOpcode();
            }

            bool isIndication() const noexcept {
                return Opcode::HANDLE_VALUE_IND == getOpcode();
            }

            std::string getName() const noexcept override {
                return "AttHandleValueRcv";
            }

        protected:
            std::string valueString() const noexcept override {
                return "handle "+jau::to_hexstring(getHandle())+", size "+std::to_string(getPDUValueSize())+", data "+view.toString();
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.7.3
     * <p>
     * ATT_HANDLE_VALUE_CFM to server, acknowledging ATT_HANDLE_VALUE_IND
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.11 Characteristic Value Indications
     * </p>
     */
    class AttHandleValueCfm: public AttPDUMsg
    {
        public:
            AttHandleValueCfm(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length)
            {
                checkOpcode(Opcode::HANDLE_VALUE_CFM);
            }

            AttHandleValueCfm()
            : AttPDUMsg(Opcode::HANDLE_VALUE_CFM, 1)
            {
            }

            /** opcode */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1; }

            std::string getName() const noexcept override {
                return "AttHandleValueCfm";
            }
    };

    /**
     * List of elements.
     *
     * { element_size, element[element_size] }, with
     *
     * element := { uint16_t startHandle, uint16_t endHandle, uint8_t value[value-size] }
     *
     */
    class AttElementList : public AttPDUMsg
    {
        protected:
            AttElementList(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length) {}

            AttElementList(const Opcode opc, const jau::nsize_t size)
            : AttPDUMsg(opc, size) {}

            virtual std::string addValueString() const { return ""; }
            virtual std::string elementString(const jau::nsize_t idx) const { (void)idx; return "not implemented"; }

            std::string valueString() const noexcept override {
                std::string res = "size "+std::to_string(getPDUValueSize())+", "+addValueString()+"elements[count "+std::to_string(getElementCount())+", "+
                        "size [total "+std::to_string(getElementSize())+", value "+std::to_string(getElementValueSize())+"]: ";
                const jau::nsize_t count = getElementCount();
                for(jau::nsize_t i=0; i<count; i++) {
                    res += std::to_string(i)+"["+elementString(i)+"],";
                }
                res += "]";
                return res;
            }

        public:
            virtual ~AttElementList() noexcept override {}

            /** Total size of one element */
            virtual jau::nsize_t getElementSize() const = 0;

            /**
             * Fixate element length
             * @param element_length
             */
            virtual void setElementSize(const uint8_t element_length) = 0;

            /**
             * Net element-value size, i.e. element size less handles.
             * <p>
             * element := { uint16_t startHandle, uint16_t endHandle, uint8_t value[value-size] }
             * </p>
             */
            virtual jau::nsize_t getElementValueSize() const = 0;

            /** Number of elements */
            constexpr_cxx20 jau::nsize_t getElementCount() const noexcept {
                /** getPDUValueSize() =
                 * - `pdu.size - getAuthSigSize() - value-offset` or
                 * - `getPDUParamSize() - getPDUValueOffset() + 1`
                 */
                return getPDUValueSize()  / getElementSize();
            }

            /**
             * Fixate element length and element count
             * @param element_length
             * @param count
             */
            void setElementCount(const jau::nsize_t count) {
                const jau::nsize_t element_length = getElementSize();
                const jau::nsize_t new_size = getPDUValueOffset() + element_length * count;
                if( pdu.size() < new_size ) {
                    throw jau::IllegalArgumentException(getName()+": "+std::to_string(getPDUValueOffset())+
                            " + element[len "+std::to_string(element_length)+
                            " * count "+std::to_string(count)+" > pdu "+std::to_string(pdu.size()), E_FILE_LINE);
                }
                pdu.resize( new_size );
                if( getPDUValueSize() % getElementSize() != 0 ) {
                    throw AttValueException(getName()+": Invalid packet size: pdu-value-size "+std::to_string(getPDUValueSize())+
                            " not multiple of element-size "+std::to_string(getElementSize()), E_FILE_LINE);
                }
            }

            jau::nsize_t getElementPDUOffset(const jau::nsize_t elementIdx) const {
                return getPDUValueOffset() + elementIdx * getElementSize();
            }

            uint8_t const * getElementPtr(const jau::nsize_t elementIdx) const {
                return pdu.get_ptr(getElementPDUOffset(elementIdx));
            }

            std::string getName() const noexcept override {
                return "AttElementList";
            }
    };

    /**
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.1 ATT_READ_BY_TYPE_REQ
     * <p>
     * ATT_READ_BY_TYPE_REQ
     * </p>
     *
     * <p>
     * and
     * </p>
     *
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.9 ATT_READ_BY_GROUP_TYPE_REQ
     * <p>
     * ATT_READ_BY_GROUP_TYPE_REQ
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.4.1 Discover All Primary Services
     * </p>
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.6.1 Discover All Characteristics of a Service
     * </p>
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.1 Characteristic Declaration Attribute Value
     * </p>
     */
    class AttReadByNTypeReq : public AttPDUMsg
    {
        private:
            jau::uuid_t::TypeSize getUUIFormat() const {
                return jau::uuid_t::toTypeSize( getPDUValueSize() );
            }

        public:
            AttReadByNTypeReq(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length)
            {
                checkOpcode(Opcode::READ_BY_GROUP_TYPE_REQ, Opcode::READ_BY_TYPE_REQ);
            }

            AttReadByNTypeReq(const bool groupTypeReq, const uint16_t startHandle, const uint16_t endHandle, const jau::uuid_t & uuid)
            : AttPDUMsg(groupTypeReq ? Opcode::READ_BY_GROUP_TYPE_REQ : Opcode::READ_BY_TYPE_REQ, getPDUValueOffset()+uuid.getTypeSizeInt())
            {
                if( uuid.getTypeSize() != jau::uuid_t::TypeSize::UUID16_SZ && uuid.getTypeSize()!= jau::uuid_t::TypeSize::UUID128_SZ ) {
                    throw jau::IllegalArgumentException("Only UUID16 and UUID128 allowed: "+uuid.toString(), E_FILE_LINE);
                }
                pdu.put_uint16_nc(1, startHandle);
                pdu.put_uint16_nc(3, endHandle);
                pdu.put_uuid_nc(5, uuid);
            }

            /** opcode + handle-start + handle-end */
            constexpr_cxx20 jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 2 + 2; }

            constexpr uint16_t getStartHandle() const noexcept { return pdu.get_uint16_nc( 1 ); }

            constexpr uint16_t getEndHandle() const noexcept { return pdu.get_uint16_nc( 1 + 2 /* 1 handle size */ ); }

            std::string getName() const noexcept override {
                return "AttReadByNTypeReq";
            }

            std::unique_ptr<const jau::uuid_t> getNType() const {
                return pdu.get_uuid( getPDUValueOffset(), getUUIFormat() );
            }

        protected:
            std::string valueString() const noexcept override {
                return "handle ["+jau::to_hexstring(getStartHandle())+".."+jau::to_hexstring(getEndHandle())+
                       "], uuid "+getNType()->toString();
            }
    };

    /**
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.2 ATT_READ_BY_TYPE_RSP
     * <p>
     * ATT_READ_BY_TYPE_RSP
     * </p>
     * <p>
     * Contains a list of elements, each comprised of handle-value pairs.
     * The handle is comprised of two octets, i.e. uint16_t.
     * <pre>
     *  element := { uint16_t handle, uint8_t value[value-size] }
     * </pre>
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.6.1 Discover All Characteristics of a Service
     * </p>
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.1 Characteristic Declaration Attribute Value
     * </p>
     */
    class AttReadByTypeRsp: public AttElementList
    {
        public:
            /**
             * element := { uint16_t handle, uint8_t value[value-size] }
             */
            class Element {
                private:
                    const jau::TOctetSlice view;

                public:
                    Element(const AttReadByTypeRsp & p, const jau::nsize_t idx)
                    : view(p.pdu, p.getElementPDUOffset(idx), p.getElementSize()) {}

                    constexpr uint16_t getHandle() const noexcept { return view.get_uint16_nc(0); }

                    constexpr uint8_t const * getValuePtr() const noexcept { return view.get_ptr_nc(2 /* handle size */); }

                    constexpr jau::nsize_t getValueSize() const noexcept { return view.size() - 2 /* handle size */; }

                    std::string toString() const {
                        return "handle "+jau::to_hexstring(getHandle())+
                               ", data "+jau::bytesHexString(getValuePtr(), 0, getValueSize(), true /* lsbFirst */);
                    }
            };

            AttReadByTypeRsp(const uint8_t* source, const jau::nsize_t length)
            : AttElementList(source, length)
            {
                checkOpcode(Opcode::READ_BY_TYPE_RSP);

                if( getPDUValueSize() % getElementSize() != 0 ) {
                    throw AttValueException("AttReadByTypeRsp: Invalid packet size: pdu-value-size "+std::to_string(getPDUValueSize())+
                            " not multiple of element-size "+std::to_string(getElementSize()), E_FILE_LINE);
                }
            }

            /** opcode + element-size */
            constexpr_cxx20 jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 1; }

            /** Returns size of each element, i.e. handle-value pair. */
            constexpr_cxx20 jau::nsize_t getElementSize() const noexcept override {
                return pdu.get_uint8_nc(1);
            }

            /**
             * Fixate element length
             * @param element_length
             */
            void setElementSize(const uint8_t element_length) override {
                pdu.put_uint8_nc(1, element_length);
            }

            /**
             * Net element-value size, i.e. element size less handle.
             * <p>
             * element := { uint16_t handle, uint8_t value[value-size] }
             * </p>
             */
            constexpr_cxx20 jau::nsize_t getElementValueSize() const noexcept override {
                return getElementSize() - 2;
            }

            /**
             * Create an incomplete response with maximal (MTU) length.
             *
             * User shall set all elements via the set*() methods
             * and finally use setElementSize() to fixate element length and element count.
             *
             * @param total_length maximum
             */
            AttReadByTypeRsp(const jau::nsize_t total_length)
            : AttElementList(Opcode::READ_BY_TYPE_RSP, total_length)
            {
                pdu.put_uint8_nc(1, 2 + 1 + 2 + 2); // dummy element_size: handle + property + handle + uuid
            }

            Element getElement(const jau::nsize_t elementIdx) const {
                return Element(*this, elementIdx);
            }

            uint16_t getElementHandle(const jau::nsize_t elementIdx) const {
                return pdu.get_uint16( getElementPDUOffset(elementIdx) );
            }
            void setElementHandle(const jau::nsize_t elementIdx, const uint16_t h) {
                pdu.put_uint16_nc( getElementPDUOffset(elementIdx), h );
            }

            uint8_t * getElementValuePtr(const jau::nsize_t elementIdx) {
                return pdu.get_wptr() + getElementPDUOffset(elementIdx) + 2 /* handle size */;
            }

            constexpr_cxx20 std::string getName() const noexcept override {
                return "AttReadByTypeRsp";
            }

        protected:
            std::string elementString(const jau::nsize_t idx) const override {
                return getElement(idx).toString();
            }
    };


    /**
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.4.10 ATT_READ_BY_GROUP_TYPE_RSP
     * <p>
     * ATT_READ_BY_GROUP_TYPE_RSP
     * </p>
     * <p>
     * Contains a list of elements, each comprised of { start_handle, end_handle, value } triple.
     * Both handle are each comprised of two octets, i.e. uint16_t.
     * <pre>
     *  element := { uint16_t startHandle, uint16_t endHandle, uint8_t value[value-size] }
     * </pre>
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.4.1 Discover All Primary Services
     * </p>
     */
    class AttReadByGroupTypeRsp : public AttElementList
    {
        public:
            /**
             * element := { uint16_t startHandle, uint16_t endHandle, uint8_t value[value-size] }
             */
            class Element {
                private:
                    const jau::TOctetSlice view;

                public:
                    Element(const AttReadByGroupTypeRsp & p, const jau::nsize_t idx)
                    : view(p.pdu, p.getElementPDUOffset(idx), p.getElementSize()) {}

                    constexpr uint16_t getStartHandle() const noexcept { return view.get_uint16_nc(0); }

                    constexpr uint16_t getEndHandle() const noexcept { return view.get_uint16_nc(2); }

                    constexpr uint8_t const * getValuePtr() const noexcept { return view.get_ptr_nc(4 /* handle size */); }

                    constexpr jau::nsize_t getValueSize() const noexcept { return view.size() - 4 /* handle size */; }
            };

            AttReadByGroupTypeRsp(const uint8_t* source, const jau::nsize_t length)
            : AttElementList(source, length)
            {
                checkOpcode(Opcode::READ_BY_GROUP_TYPE_RSP);

                if( getPDUValueSize() % getElementSize() != 0 ) {
                    throw AttValueException("AttReadByGroupTypeRsp: Invalid packet size: pdu-value-size "+std::to_string(getPDUValueSize())+
                            " not multiple of element-size "+std::to_string(getElementSize()), E_FILE_LINE);
                }
            }

            /** opcode + element-size */
            constexpr_cxx20 jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 1; }

            /** Returns size of each element, i.e. handle-value triple. */
            constexpr_cxx20 jau::nsize_t getElementSize() const noexcept override {
                return pdu.get_uint8_nc(1);
            }

            /**
             * Fixate element length
             * @param element_length
             */
            void setElementSize(const uint8_t element_length) override {
                pdu.put_uint8_nc(1, element_length);
            }

            /**
             * Net element-value size, i.e. element size less handles.
             * <p>
             * element := { uint16_t startHandle, uint16_t endHandle, uint8_t value[value-size] }
             * </p>
             */
            constexpr_cxx20 jau::nsize_t getElementValueSize() const noexcept override {
                return getElementSize() - 4;
            }

            /**
             * Create an incomplete response with maximal (MTU) length.
             *
             * User shall set all elements via the set*() methods
             * and finally use setElementSize() to fixate element length and element count.
             *
             * @param total_length maximum
             */
            AttReadByGroupTypeRsp(const jau::nsize_t total_length)
            : AttElementList(Opcode::READ_BY_GROUP_TYPE_RSP, total_length)
            {
                pdu.put_uint8_nc(1, 2+2+2); // dummy element_size: handle + handle + uuid
            }

            Element getElement(const jau::nsize_t elementIdx) const {
                return Element(*this, elementIdx);
            }

            uint16_t getElementStartHandle(const jau::nsize_t elementIdx) const {
                return pdu.get_uint16( getElementPDUOffset(elementIdx) );
            }
            void setElementStartHandle(const jau::nsize_t elementIdx, const uint16_t h) {
                pdu.put_uint16_nc( getElementPDUOffset(elementIdx), h );
            }

            uint16_t getElementEndHandle(const jau::nsize_t elementIdx) const {
                return pdu.get_uint16( getElementPDUOffset(elementIdx) + 2 /* 1 handle size */ );
            }
            void setElementEndHandle(const jau::nsize_t elementIdx, const uint16_t h) {
                pdu.put_uint16_nc( getElementPDUOffset(elementIdx) + 2, h );
            }

            uint8_t * getElementValuePtr(const jau::nsize_t elementIdx) {
                return pdu.get_wptr() + getElementPDUOffset(elementIdx) + 4 /* 2 handle size */;
            }
            void setElementValueUUID(const jau::nsize_t elementIdx, const jau::uuid_t& v) {
                uint8_t * b = getElementValuePtr(elementIdx);
                v.put(b, 0, true /* littleEndian */);
            }

            std::string getName() const noexcept override {
                return "AttReadByGroupTypeRsp";
            }

        protected:
            std::string elementString(const jau::nsize_t idx) const override {
                Element e = getElement(idx);
                return "handle ["+jau::to_hexstring(e.getStartHandle())+".."+jau::to_hexstring(e.getEndHandle())+
                       "], data "+jau::bytesHexString(e.getValuePtr(), 0, e.getValueSize(), true /* lsbFirst */);
            }
    };

    /**
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.3.1 ATT_FIND_INFORMATION_REQ
     * <p>
     * ATT_FIND_INFORMATION_REQ
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.7.1 Discover All Characteristic Descriptors
     * </p>
     */
    class AttFindInfoReq : public AttPDUMsg
    {
        public:
            AttFindInfoReq(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length)
            {
                checkOpcode(Opcode::FIND_INFORMATION_REQ);
            }

            AttFindInfoReq(const uint16_t startHandle, const uint16_t endHandle)
            : AttPDUMsg(Opcode::FIND_INFORMATION_REQ, 1+2+2)
            {
                pdu.put_uint16_nc(1, startHandle);
                pdu.put_uint16_nc(3, endHandle);
            }

            /** opcode + handle_start + handle_end */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 2 + 2; }

            constexpr uint16_t getStartHandle() const noexcept { return pdu.get_uint16_nc( 1 ); }

            constexpr uint16_t getEndHandle() const noexcept { return pdu.get_uint16_nc( 1 + 2 ); }

            constexpr_cxx20 std::string getName() const noexcept override {
                return "AttFindInfoReq";
            }

        protected:
            std::string valueString() const noexcept override {
                return "handle ["+jau::to_hexstring(getStartHandle())+".."+jau::to_hexstring(getEndHandle())+"]";
            }
    };

    /**
     * BT Core Spec v5.2: Vol 3, Part F ATT: 3.4.3.2 ATT_FIND_INFORMATION_RSP
     * <p>
     * ATT_FIND_INFORMATION_RSP
     * </p>
     * <p>
     * Contains a list of elements, each comprised of { handle, [UUID16 | UUID128] } pair.
     * The handle is comprised of two octets, i.e. uint16_t.
     * The UUID is either comprised of 2 octets for UUID16 or 16 octets for UUID128
     * depending on the given format.
     * <pre>
     *  element := { uint16_t handle, UUID value }, with a UUID of UUID16 or UUID128
     * </pre>
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.7.1 Discover All Characteristic Descriptors
     * </p>
     */
    class AttFindInfoRsp: public AttElementList
    {
        private:
            static jau::uuid_t::TypeSize toTypeSize(const uint8_t format) {
                if( 0x01 == format ) {
                    return jau::uuid_t::TypeSize::UUID16_SZ;
                } else if( 0x02 == format ) {
                    return jau::uuid_t::TypeSize::UUID128_SZ;
                }
                throw AttValueException("AttFindInfoRsp: Invalid format "+std::to_string(format)+", not UUID16 (1) or UUID128 (2)", E_FILE_LINE);
            }

            static uint8_t toFormatCode(const jau::uuid_t::TypeSize tsz) {
                switch( tsz ) {
                    case jau::uuid_t::TypeSize::UUID16_SZ: return 0x01;
                    case jau::uuid_t::TypeSize::UUID128_SZ: return 0x02;
                    default: break;
                }
                throw AttValueException("AttFindInfoRsp: Invalid TypeSize "+jau::uuid_t::getTypeSizeString(tsz)+", not UUID16_SZ (1) or UUID128_SZ (2)", E_FILE_LINE);
            }

        public:
            /**
             * element := { uint16_t handle, UUID value }, with a UUID of UUID16 or UUID128
             */
            class Element {
                public:
                    const uint16_t handle;
                    const std::unique_ptr<const jau::uuid_t> uuid;

                    Element(const AttFindInfoRsp & p, const jau::nsize_t idx)
                    : handle( p.getElementHandle(idx) ), uuid( p.getElementValue(idx) )
                    { }
            };

            AttFindInfoRsp(const uint8_t* source, const jau::nsize_t length)
            : AttElementList(source, length)
            {
                checkOpcode(Opcode::FIND_INFORMATION_RSP);
                if( getPDUValueSize() % getElementSize() != 0 ) {
                    throw AttValueException("AttFindInfoRsp: Invalid packet size: pdu-value-size "+std::to_string(getPDUValueSize())+
                            " not multiple of element-size "+std::to_string(getElementSize()), E_FILE_LINE);
                }
            }

            /** opcode + format */
            constexpr_cxx20 jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 1; }

            /**
             * Returns element size.
             * <p>
             * element := { uint16_t handle, UUID value }, with a UUID of UUID16 or UUID128
             * </p>
             */
            jau::nsize_t getElementSize() const override {
                return 2 /* handle */ + getElementValueSize();
            }

            jau::uuid_t::TypeSize getElementValueFormat() const {
                return toTypeSize( pdu.get_uint8_nc(1) );
            }

            /**
             * Net element-value size, i.e. element size less handles.
             * <p>
             * element := { uint16_t handle, UUID value }, with a UUID of UUID16 or UUID128
             * </p>
             */
            jau::nsize_t getElementValueSize() const override {
                return jau::uuid_t::number(getElementValueFormat());
            }

            /**
             * Fixate element length
             * @param element_length
             */
            void setElementSize(const uint8_t element_length) override {
                const jau::nsize_t tsz_i = element_length - 2 /* handle */;
                const jau::uuid_t::TypeSize tsz = jau::uuid_t::toTypeSize(tsz_i);
                pdu.put_uint8_nc(1, toFormatCode(tsz));
            }

            /**
             * Create an incomplete response with maximal (MTU) length.
             *
             * User shall set all elements via the set*() methods
             * and finally use setElementSize() to fixate element length and element count.
             *
             * @param total_length maximum
             */
            AttFindInfoRsp(const jau::nsize_t total_length)
            : AttElementList(Opcode::FIND_INFORMATION_RSP, total_length)
            {
                pdu.put_uint8_nc(1, 2); // dummy format: uuid16_t
            }

            Element getElement(const jau::nsize_t elementIdx) const {
                return Element(*this, elementIdx);
            }

            uint16_t getElementHandle(const jau::nsize_t elementIdx) const {
                return pdu.get_uint16( getElementPDUOffset(elementIdx) );
            }
            void setElementHandle(const jau::nsize_t elementIdx, const uint16_t h) {
                pdu.put_uint16_nc( getElementPDUOffset(elementIdx), h );
            }

            std::unique_ptr<const jau::uuid_t> getElementValue(const jau::nsize_t elementIdx) const {
                return pdu.get_uuid( getElementPDUOffset(elementIdx) + 2, getElementValueFormat() );
            }
            void setElementValueUUID(const jau::nsize_t elementIdx, const jau::uuid_t& v) {
                uint8_t * b = pdu.get_wptr() + getElementPDUOffset(elementIdx) + 2 /* handle size */;
                v.put(b, 0, true /* littleEndian */);
            }

            constexpr_cxx20 std::string getName() const noexcept override {
                return "AttFindInfoRsp";
            }

        protected:
            std::string addValueString() const override { return "format "+std::to_string(pdu.get_uint8_nc(1))+", "; }

            std::string elementString(const jau::nsize_t idx) const override {
                Element e = getElement(idx);
                return "handle "+jau::to_hexstring(e.handle)+
                       ", uuid "+e.uuid.get()->toString();
            }
    };
}

/** \example dbt_scanner10.cpp
 * This _dbt_scanner10_ C++ scanner ::BTRole::Master example uses the Direct-BT fully event driven workflow
 * and adds multithreading, i.e. one thread processes each found device found
 * as notified via the event listener.
 *
 * _dbt_scanner10_ represents the recommended utilization of Direct-BT.
 *
 * ### dbt_scanner10 Invocation Examples:
 * Using `scripts/run-dbt_scanner10.sh` from `dist` directory:
 * * Scan and read all devices (using default auto-sec w/ keyboard iocap)
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh
 *   ~~~
 *
 * * Read device C0:26:DA:01:DA:B1  (using default auto-sec w/ keyboard iocap)
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh -dev C0:26:DA:01:DA:B1
 *   ~~~
 *
 * * Read device C0:26:DA:01:DA:B1  (using default auto-sec w/ keyboard iocap) from adapter 01:02:03:04:05:06
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh -adapter adapter 01:02:03:04:05:06 -dev C0:26:DA:01:DA:B1
 *   ~~~
 *
 * * Read device C0:26:DA:01:DA:B1  (enforcing no security)
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh -dev C0:26:DA:01:DA:B1 -seclevel C0:26:DA:01:DA:B1 1
 *   ~~~
 *
 * * Read any device containing C0:26:DA  (enforcing no security)
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh -dev C0:26:DA -seclevel C0:26:DA 1
 *   ~~~
 *
 * * Read any device containing name `TAIDOC` (enforcing no security)
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh -dev 'TAIDOC' -seclevel 'TAIDOC' 1
 *   ~~~
 *
 * * Read device C0:26:DA:01:DA:B1, basic debug flags enabled (using default auto-sec w/ keyboard iocap)
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh -dev C0:26:DA:01:DA:B1 -dbt_debug true
 *   ~~~
 *
 * * Read device C0:26:DA:01:DA:B1, all debug flags enabled (using default auto-sec w/ keyboard iocap)
 *   ~~~
 *   ../scripts/run-dbt_scanner10.sh -dev C0:26:DA:01:DA:B1 -dbt_debug adapter.event,gatt.data,hci.event,hci.scan_ad_eir,mgmt.event
 *   ~~~
 *
 * ## Special Actions
 * * To do a BT adapter removal/add via software, assuming the device is '1-4' (Bus 1.Port 4):
 *   ~~~
 *   echo '1-4' > /sys/bus/usb/drivers/usb/unbind
 *   echo '1-4' > /sys/bus/usb/drivers/usb/bind
 *   ~~~
 */

/** \example dbt_peripheral00.cpp
 * This _dbt_peripheral00__ C++ peripheral ::BTRole::Slave example uses the Direct-BT fully event driven workflow.
 */

 /** \example dbt_scanner00.cpp
  * This C++ direct_bt scanner example is a TinyB backward compatible and not fully event driven.
  * It uses a more simple high-level approach via semantic GATT types (Service, Characteristic, ..)
  * without bothering with fine implementation details of GATTHandler.
  * <p>
  * For a more technical and low-level approach see dbt_scanner01.cpp!
  * </p>
  * <p>
  * This example does not represent the recommended utilization of Direct-BT.
  * </p>
  */

 /** \example dbt_scanner01.cpp
  * This C++ direct_bt scanner example is a TinyB backward compatible and not fully event driven.
  * It uses a more fine grained control via GATTHandler.
  * <p>
  * For a more user convenient and readable approach see dbt_scanner00.cpp or dbt_scanner10.cpp!
  * </p>
  * <p>
  * This example does not represent the recommended utilization of Direct-BT.
  * </p>
  */

#endif /* ATT_PDU_TYPES_HPP_ */
