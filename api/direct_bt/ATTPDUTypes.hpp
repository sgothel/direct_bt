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

#include "UUID.hpp"
#include "BTTypes.hpp"

#include "OctetTypes.hpp"

/**
 * Direct-BT provides direct Bluetooth LE and BREDR programming,
 * offering robust high-performance support for embedded & desktop with zero overhead via C++ and Java.
 *
 * Direct-BT supports a fully event driven workflow from device discovery to GATT programming,
 * using its platform agnostic HCI, GATT, SMP and L2CAP client-side protocol implementation.
 *
 * Direct-BT implements the following layers
 * - DBTManager for adapter configuration and adapter add/removal notifications (ChangedAdapterSetFunc())
 *   - Using *BlueZ Kernel Manager Control Channel* via MgmtMsg communication.
 * - *HCI Handling* via HCIHandler using HCIPacket implementing connect/disconnect w/ tracking, device discovery, etc
 * - *ATT PDU* AttPDUMsg via L2CAP for low level packet communication
 * - *GATT Support* via GATTHandler using AttPDUMsg over L2CAPComm, providing
 *   -  GATTService
 *   -  GATTCharacteristic
 *   -  GATTDescriptor
 * - *SMP PDU* SMPPDUMsg via L2CAP for Security Manager Protocol (SMP) communication
 * - *SMP Support* via SMPHandler using SMPPDUMsg over L2CAPComm, providing (Not yet supported by Linux/BlueZ)
 *   - LE Secure Connections
 *   - LE legacy pairing
 * - On Linux/BlueZ, LE Secure Connections and LE legacy pairing is supported using
 *   - ::BTSecurityLevel setting via DBTDevice / L2CAPComm per connection and
 *   - ::SMPIOCapability via DBTManager (per adapter) and DBTDevice (per connection)
 *   - SMPPDUMsg SMP event tracking over HCI/ACL/L2CAP, observing operations
 *
 * DBTManager utilizes the *BlueZ Kernel Manager Control Channel*
 * for adapter configuration and adapter add/removal notifications (ChangedAdapterSetFunc()).
 *
 * To support other platforms than Linux/BlueZ, we will have to
 * - Move specified HCI host features used in DBTManager to HCIHandler, SMPHandler,..  - and -
 * - Add specialization for each new platform using their non-platform-agnostic features.
 *
 * - - - - - - - - - - - - - - -
 *
 * From a user perspective the following hierarchy is provided
 * - DBTManager has zero or more
 *   - DBTAdapter has zero or more
 *     - DBTDevice has zero or more
 *       - GATTService has zero or more
 *         - GATTCharacteristic has zero or more
 *           - GATTDescriptor
 *
 * - - - - - - - - - - - - - - -
 *
 * Object lifecycle with all instances and marked weak back-references to their owner
 * - DBTManager singleton instance for all
 * - DBTAdapter ownership by DBTManager
 *   - DBTDevice ownership by DBTAdapter
 *     - GATTHandler ownership by DBTDevice, with weak DBTDevice back-reference
 *       - GATTService ownership by GATTHandler, with weak GATTHandler back-reference
 *         - GATTCharacteristic ownership by GATTService, with weak GATTService back-reference
 *           - GATTDescriptor ownership by GATTCharacteristic, with weak GATTCharacteristic back-reference
 *
 * - - - - - - - - - - - - - - -
 *
 * Mapped names from C++ implementation to Java implementation and to Java interface:
 *
 *  C++                | Java Implementation   | Java Interface |
 *  :------------------| :---------------------| :---------------------------|
 *  DBTManager         | DBTManager            | BluetoothManager            |
 *  DBTAdapter         | DBTAdapter            | BluetoothAdapter            |
 *  DBTDevice          | DBTDevice             | BluetoothDevice             |
 *  GATTService        | DBTGattService        | BluetoothGattService        |
 *  GATTCharacteristic | DBTGattCharacteristic | BluetoothGattCharacteristic |
 *  GATTDescriptor     | DBTGattDescriptor     | BluetoothGattDescriptor     |
 *
 * - - - - - - - - - - - - - - -
 *
 * A fully event driven workflow from discovery to GATT programming is supported.
 *
 * AdapterStatusListener allows listening to adapter changes and device discovery
 * and GATTCharacteristicListener to GATT indications and notifications.
 *
 * Main event listener can be attached to these objects
 * which maintain a set of unique listener instances without duplicates.
 *
 * - DBTAdapter
 *   - AdapterStatusListener
 *
 * - GATTHandler
 *   - GATTCharacteristicListener
 *
 * Other API attachment method exists for GATTCharacteristicListener,
 * however, they only exists for convenience and end up to be attached to GATTHandler.
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
     * <p>
     * Little endian, however, ATT value endianess is defined by above layer.
     * </p>
     * <p>
     * ATT_MTU Vol 3, Part F 3.2.8
     * Maximum size of any packet sent. Higher layer spec defines the default ATT_MTU value.
     * LE L2CAP GATT ATT_MTU is 23 bytes (Vol 3, Part G 5.2.1).
     *
     * Maximum length of an attribute value shall be 512 bytes (Vol 3, Part F 3.2.8),
     * spread across multiple PDUs.
     * </p>
     * <p>
     * ATT PDU Format Vol 3, Part F 3.3.1
     * -----------------------------------
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
                    throw AttOpcodeException("Has opcode "+jau::uint8HexString(number(has), true)+" "+getOpcodeString(has)+
                                     ", but expected "+jau::uint8HexString(number(expected), true)+" "+getOpcodeString(expected), E_FILE_LINE);
                }
            }
            void checkOpcode(const Opcode exp1, const Opcode exp2) const
            {
                const Opcode has = getOpcode();
                if( exp1 != has && exp2 != has ) {
                    throw AttOpcodeException("Has opcode "+jau::uint8HexString(number(has), true)+" "+getOpcodeString(has)+
                                     ", but expected either "+jau::uint8HexString(number(exp1), true)+" "+getOpcodeString(exp1)+
                                     " or  "+jau::uint8HexString(number(exp1), true)+" "+getOpcodeString(exp1), E_FILE_LINE);
                }
            }

            virtual std::string baseString() const noexcept {
                return "opcode="+jau::uint8HexString(number(getOpcode()), true)+" "+getOpcodeString()+
                        ", size[total="+std::to_string(pdu.getSize())+", param "+std::to_string(getPDUParamSize())+"]";
            }
            virtual std::string valueString() const noexcept {
                return "size "+std::to_string(getPDUValueSize())+", data "
                        +jau::bytesHexString(pdu.get_ptr(), getPDUValueOffset(), getPDUValueSize(), true /* lsbFirst */, true /* leading0X */);
            }

        public:
            /** actual received PDU */
            POctets pdu;

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
                : pdu(source, std::max<jau::nsize_t>(1, size)), ts_creation(jau::getCurrentMilliseconds())
            {
                pdu.check_range(0, getPDUMinSize());
            }

            /** Persistent memory, w/ ownership ..*/
            AttPDUMsg(const Opcode opc, const jau::nsize_t size)
                : pdu(std::max<jau::nsize_t>(1, size)), ts_creation(jau::getCurrentMilliseconds())
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
            inline Opcode getOpcode() const noexcept {
                return static_cast<Opcode>(pdu.get_uint8_nc(0));
            }
            std::string getOpcodeString() const noexcept { return getOpcodeString(getOpcode()); }

            /** ATT PDU Format Vol 3, Part F 3.3.1 */
            Opcode getOpMethod() const noexcept {
                return bit_and(getOpcode(), Opcode::METHOD_MASK);
            }

            /** ATT PDU Format Vol 3, Part F 3.3.1 */
            bool getOpCommandFlag() const noexcept {
                return bit_test(getOpcode(), Opcode::COMMAND_FLAG);
            }

            /** ATT PDU Format Vol 3, Part F 3.3.1 */
            bool getOpAuthSigFlag() const noexcept {
                return bit_test(getOpcode(), Opcode::AUTH_SIGNATURE_FLAG);
            }

            /**
             * ATT PDU Format Vol 3, Part F 3.3.1
             * <p>
             * The ATT Authentication Signature size in octets.
             * </p>
             * <p>
             * This auth-signature comes at the very last of the PDU.
             * </p>
             */
            jau::nsize_t getAuthSigSize() const noexcept {
                return getOpAuthSigFlag() ? 12 : 0;
            }

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
            jau::nsize_t getPDUParamSize() const noexcept {
                return pdu.getSize() - getAuthSigSize() - 1 /* opcode */;
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
            virtual jau::nsize_t getPDUValueOffset() const noexcept { return 1; /* default: opcode */ }

            /**
             * Returns this PDU's minimum size, i.e.
             * <pre>
             *   opcode + param - value + auth_signature
             * </pre>
             * Value is excluded as it might be flexible.
             */
            jau::nsize_t getPDUMinSize() const noexcept {
                return getPDUValueOffset() + getAuthSigSize();
            }

            /**
             * Returns the octet size of the value attributes in this PDI,
             * i.e. getPDUParamSize() - getPDUValueOffset() + 1.
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
            jau::nsize_t getPDUValueSize() const noexcept { return getPDUParamSize() - getPDUValueOffset() + 1; }

            /**
             * Returns the theoretical maximum value size of a PDU.
             * <pre>
             *  ATT_MTU - getAuthSigSize() - value-offset
             * </pre>
             */
            jau::nsize_t getMaxPDUValueSize(const jau::nsize_t mtu) const noexcept {
                return mtu - getAuthSigSize() - getPDUValueOffset();
            }

            virtual std::string getName() const noexcept {
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

            /** opcode */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1; }

            std::string getName() const noexcept override {
                return "AttPDUUndefined";
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.1.1
     * <p>
     * ATT_ERROR_RSP (ATT Opcode 0x01)
     * </p>
     */
    class AttErrorRsp: public AttPDUMsg
    {
        public:
            enum class ErrorCode : uint8_t {
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
            static std::string getPlainErrorString(const ErrorCode errorCode) noexcept;

            AttErrorRsp(const uint8_t* source, const jau::nsize_t length) : AttPDUMsg(source, length) {
                checkOpcode(Opcode::ERROR_RSP);
            }

            /** opcode + reqOpcodeCause + handleCause + errorCode */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 1 + 2 + 1; }

            uint8_t getRequestedOpcodeCause() const noexcept {
                return pdu.get_uint8_nc(1);
            }

            uint16_t getHandleCause() const noexcept {
                return pdu.get_uint16_nc(2);
            }

            ErrorCode getErrorCode() const noexcept {
                return static_cast<ErrorCode>(pdu.get_uint8_nc(4));
            }

            std::string getErrorString() const noexcept {
                const ErrorCode ec = getErrorCode();
                return jau::uint8HexString(number(ec), true) + ": " + getPlainErrorString(ec);
            }

            std::string getName() const noexcept override {
                return "AttErrorRsp";
            }

        protected:
            std::string valueString() const noexcept override {
                return getErrorString();
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.2.2
     * <p>
     * ATT_EXCHANGE_MTU_REQ, ATT_EXCHANGE_MTU_RSP
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.3.1 Exchange MTU (Server configuration)
     * </p>
     */
    class AttExchangeMTU: public AttPDUMsg
    {
        public:
            AttExchangeMTU(const uint8_t* source, const jau::nsize_t length) : AttPDUMsg(source, length) {
                checkOpcode(Opcode::EXCHANGE_MTU_RSP);
            }

            AttExchangeMTU(const uint16_t mtuSize)
            : AttPDUMsg(Opcode::EXCHANGE_MTU_REQ, 1+2)
            {
                pdu.put_uint16_nc(1, mtuSize);
            }

            /** opcode + mtu-size */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1+2; }

            uint16_t getMTUSize() const noexcept {
                return pdu.get_uint16_nc(1);
            }

            std::string getName() const noexcept override {
                return "AttExchangeMTU";
            }

        protected:
            std::string valueString() const noexcept override {
                return "mtu "+std::to_string(getMTUSize());
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.4.3
     * <p>
     * ATT_READ_REQ
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.1 Read Characteristic Value
     * </p>
     */
    class AttReadReq : public AttPDUMsg
    {
        public:
            AttReadReq(const uint16_t handle)
            : AttPDUMsg(Opcode::READ_REQ, 1+2)
            {
                pdu.put_uint16_nc(1, handle);
            }

            /** opcode + handle */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1+2; }

            uint16_t getHandle() const noexcept {
                return pdu.get_uint16_nc( 1 );
            }

            std::string getName() const noexcept override {
                return "AttReadReq";
            }

        protected:
            std::string valueString() const noexcept override {
                return "handle "+jau::uint16HexString(getHandle(), true);
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.4.4
     * <p>
     * ATT_READ_RSP (ATT Opcode 0x0B)
     * </p>
     * <p>
     * If expected value size exceeds getValueSize(), continue with ATT_READ_BLOB_REQ (3.4.4.5),
     * see shallReadBlobReq(..)
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.1 Read Characteristic Value
     * </p>
     */
    class AttReadRsp: public AttPDUMsg
    {
        private:
            const TOctetSlice view;

        public:
            static bool instanceOf();

            AttReadRsp(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length), view(pdu, getPDUValueOffset(), getPDUValueSize()) {
                checkOpcode(Opcode::READ_RSP);
            }

            /** opcode */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1; }

            uint8_t const * getValuePtr() const noexcept { return pdu.get_ptr_nc(getPDUValueOffset()); }

            TOctetSlice const & getValue() const noexcept { return view; }

            std::string getName() const noexcept override {
                return "AttReadRsp";
            }

        protected:
            std::string valueString() const noexcept override {
                return "size "+std::to_string(getPDUValueSize())+", data "+view.toString();
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.4.5
     * <p>
     * ATT_READ_BLOB_REQ
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.3 Read Long Characteristic Value
     * </p>
     */
    class AttReadBlobReq : public AttPDUMsg
    {
        public:
            AttReadBlobReq(const uint16_t handle, const uint16_t value_offset)
            : AttPDUMsg(Opcode::READ_BLOB_REQ, 1+2+2)
            {
                pdu.put_uint16_nc(1, handle);
                pdu.put_uint16_nc(3, value_offset);
            }

            /** opcode + handle + value_offset */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 2 + 2; }

            uint16_t getHandle() const noexcept {
                return pdu.get_uint16_nc( 1 );
            }

            uint16_t getValueOffset() const noexcept {
                return pdu.get_uint16_nc( 1 + 2 );
            }

            std::string getName() const noexcept override {
                return "AttReadBlobReq";
            }

        protected:
            std::string valueString() const noexcept override {
                return "handle "+jau::uint16HexString(getHandle(), true)+", valueOffset "+jau::uint16HexString(getValueOffset(), true);
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.4.6
     * <p>
     * ATT_READ_BLOB_RSP
     * </p>
     * <p>
     * If expected value size exceeds getValueSize(), continue with ATT_READ_BLOB_REQ (3.4.4.5),
     * see shallReadBlobReq(..)
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.8.3 Read Long Characteristic Value
     * </p>
     */
    class AttReadBlobRsp: public AttPDUMsg
    {
        private:
            const TOctetSlice view;

        public:
            static bool instanceOf();

            AttReadBlobRsp(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length), view(pdu, getPDUValueOffset(), getPDUValueSize()) {
                checkOpcode(Opcode::READ_BLOB_RSP);
            }

            /** opcode */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1; }

            uint8_t const * getValuePtr() const noexcept { return pdu.get_ptr_nc(getPDUValueOffset()); }

            TOctetSlice const & getValue() const noexcept { return view; }

            std::string getName() const noexcept override {
                return "AttReadBlobRsp";
            }

        protected:
            std::string valueString() const noexcept override {
                return "size "+std::to_string(getPDUValueSize())+", data "+view.toString();
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.5.1
     * <p>
     * ATT_WRITE_REQ
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
     * </p>
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 3.3.3.3 Client Characteristic Configuration
     * </p>
     */
    class AttWriteReq : public AttPDUMsg
    {
        private:
            const TOctetSlice view;

        public:
            AttWriteReq(const uint16_t handle, const TROOctets & value)
            : AttPDUMsg(Opcode::WRITE_REQ, 1+2+value.getSize()), view(pdu, getPDUValueOffset(), getPDUValueSize())
            {
                pdu.put_uint16_nc(1, handle);
                for(jau::nsize_t i=0; i<value.getSize(); i++) {
                    pdu.put_uint8_nc(3+i, value.get_uint8_nc(i));
                }
            }

            /** opcode + handle */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 2; }

            uint16_t getHandle() const noexcept {
                return pdu.get_uint16_nc( 1 );
            }

            uint8_t const * getValuePtr() const noexcept { return pdu.get_ptr_nc(getPDUValueOffset()); }

            TOctetSlice const & getValue() const noexcept { return view; }

            std::string getName() const noexcept override {
                return "AttWriteReq";
            }

        protected:
            std::string valueString() const noexcept override {
                return "handle "+jau::uint16HexString(getHandle(), true)+", data "+view.toString();;
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.5.2
     * <p>
     * ATT_WRITE_RSP
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.3 Write Characteristic Value
     * </p>
     */
    class AttWriteRsp : public AttPDUMsg
    {
        public:
            AttWriteRsp(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length) {
                checkOpcode(Opcode::WRITE_RSP);
            }

            /** opcode */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1; }

            std::string getName() const noexcept override {
                return "AttWriteRsp";
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.5.3
     * <p>
     * ATT_WRITE_CMD
     * </p>
     * Used in:
     * <p>
     * BT Core Spec v5.2: Vol 3, Part G GATT: 4.9.1 Write Characteristic Value without Response
     * </p>
     */
    class AttWriteCmd : public AttPDUMsg
    {
        private:
            const TOctetSlice view;

        public:
            AttWriteCmd(const uint16_t handle, const TROOctets & value)
            : AttPDUMsg(Opcode::WRITE_CMD, 1+2+value.getSize()), view(pdu, getPDUValueOffset(), getPDUValueSize())
            {
                pdu.put_uint16_nc(1, handle);
                for(jau::nsize_t i=0; i<value.getSize(); i++) {
                    pdu.put_uint8_nc(3+i, value.get_uint8_nc(i));
                }
            }

            /** opcode + handle */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 2; }

            uint16_t getHandle() const noexcept {
                return pdu.get_uint16_nc( 1 );
            }

            uint8_t const * getValuePtr() const noexcept { return pdu.get_ptr_nc(getPDUValueOffset()); }

            TOctetSlice const & getValue() const noexcept { return view; }

            std::string getName() const noexcept override {
                return "AttWriteCmd";
            }

        protected:
            std::string valueString() const noexcept override {
                return "handle "+jau::uint16HexString(getHandle(), true)+", data "+view.toString();;
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
            const TOctetSlice view;

        public:
            AttHandleValueRcv(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length), view(pdu, getPDUValueOffset(), getPDUValueSize()) {
                checkOpcode(Opcode::HANDLE_VALUE_NTF, Opcode::HANDLE_VALUE_IND);
            }

            /** opcode + handle */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1+2; }

            uint16_t getHandle() const noexcept {
                return pdu.get_uint16_nc(1);
            }

            uint8_t const * getValuePtr() const noexcept { return pdu.get_ptr_nc(getPDUValueOffset()); }

            TOctetSlice const & getValue() const noexcept { return view; }

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
                return "handle "+jau::uint16HexString(getHandle(), true)+", size "+std::to_string(getPDUValueSize())+", data "+view.toString();
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

    class AttElementList : public AttPDUMsg
    {
        protected:
            AttElementList(const uint8_t* source, const jau::nsize_t length)
            : AttPDUMsg(source, length) {}

            virtual std::string addValueString() const { return ""; }
            virtual std::string elementString(const jau::nsize_t idx) const { (void)idx; return "not implemented"; }

            std::string valueString() const noexcept override {
                std::string res = "size "+std::to_string(getPDUValueSize())+", "+addValueString()+"elements[count "+std::to_string(getElementCount())+", "+
                        "size [total "+std::to_string(getElementTotalSize())+", value "+std::to_string(getElementValueSize())+"]: ";
                const jau::nsize_t count = getElementCount();
                for(jau::nsize_t i=0; i<count; i++) {
                    res += std::to_string(i)+"["+elementString(i)+"],";
                }
                res += "]";
                return res;
            }

        public:
            virtual ~AttElementList() noexcept override {}

            virtual jau::nsize_t getElementTotalSize() const = 0;
            virtual jau::nsize_t getElementValueSize() const = 0;
            virtual jau::nsize_t getElementCount() const = 0;

            jau::nsize_t getElementPDUOffset(const jau::nsize_t elementIdx) const {
                return getPDUValueOffset() + elementIdx * getElementTotalSize();
            }

            uint8_t const * getElementPtr(const jau::nsize_t elementIdx) const {
                return pdu.get_ptr(getElementPDUOffset(elementIdx));
            }

            std::string getName() const noexcept override {
                return "AttElementList";
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.4.1
     * <p>
     * ATT_READ_BY_TYPE_REQ
     * </p>
     *
     * <p>
     * and
     * </p>
     *
     * ATT Protocol PDUs Vol 3, Part F 3.4.4.9
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
            uuid_t::TypeSize getUUIFormat() const {
                return uuid_t::toTypeSize(this->getPDUValueSize());
            }

        public:
            AttReadByNTypeReq(const bool groupTypeReq, const uint16_t startHandle, const uint16_t endHandle, const uuid_t & uuid)
            : AttPDUMsg(groupTypeReq ? Opcode::READ_BY_GROUP_TYPE_REQ : Opcode::READ_BY_TYPE_REQ, 1+2+2+uuid.getTypeSizeInt())
            {
                if( uuid.getTypeSize() != uuid_t::TypeSize::UUID16_SZ && uuid.getTypeSize()!= uuid_t::TypeSize::UUID128_SZ ) {
                    throw jau::IllegalArgumentException("Only UUID16 and UUID128 allowed: "+uuid.toString(), E_FILE_LINE);
                }
                pdu.put_uint16_nc(1, startHandle);
                pdu.put_uint16_nc(3, endHandle);
                pdu.put_uuid_nc(5, uuid);
            }

            /** opcode + handle-start + handle-end */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 2 + 2; }

            uint16_t getStartHandle() const noexcept {
                return pdu.get_uint16_nc( 1 );
            }

            uint16_t getEndHandle() const noexcept {
                return pdu.get_uint16_nc( 1 + 2 /* 1 handle size */ );
            }

            std::string getName() const noexcept override {
                return "AttReadByNTypeReq";
            }

            std::unique_ptr<const uuid_t> getNType() const {
                return pdu.get_uuid( getPDUValueOffset(), getUUIFormat() );
            }

        protected:
            std::string valueString() const noexcept override {
                return "handle ["+jau::uint16HexString(getStartHandle(), true)+".."+jau::uint16HexString(getEndHandle(), true)+
                       "], uuid "+getNType()->toString();
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.4.2
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
                    const TOctetSlice view;

                public:
                    Element(const AttReadByTypeRsp & p, const jau::nsize_t idx)
                    : view(p.pdu, p.getElementPDUOffset(idx), p.getElementTotalSize()) {}

                    uint16_t getHandle() const noexcept {
                        return view.get_uint16_nc(0);
                    }

                    uint8_t const * getValuePtr() const noexcept {
                        return view.get_ptr_nc(2 /* handle size */);
                    }

                    jau::nsize_t getValueSize() const noexcept { return view.getSize() - 2 /* handle size */; }

                    std::string toString() const {
                        return "handle "+jau::uint16HexString(getHandle(), true)+
                               ", data "+jau::bytesHexString(getValuePtr(), 0, getValueSize(), true /* lsbFirst */, true /* leading0X */);
                    }
            };

            AttReadByTypeRsp(const uint8_t* source, const jau::nsize_t length)
            : AttElementList(source, length)
            {
                checkOpcode(Opcode::READ_BY_TYPE_RSP);

                if( getPDUValueSize() % getElementTotalSize() != 0 ) {
                    throw AttValueException("PDUReadByTypeRsp: Invalid packet size: pdu-value-size "+std::to_string(getPDUValueSize())+
                            " not multiple of element-size "+std::to_string(getElementTotalSize()), E_FILE_LINE);
                }
            }

            /** opcode + element-size */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 1; }

            /** Returns size of each element, i.e. handle-value pair. */
            jau::nsize_t getElementTotalSize() const noexcept override {
                return pdu.get_uint8_nc(1);
            }

            /**
             * Net element-value size, i.e. element size less handle.
             * <p>
             * element := { uint16_t handle, uint8_t value[value-size] }
             * </p>
             */
            jau::nsize_t getElementValueSize() const noexcept override {
                return getElementTotalSize() - 2;
            }

            jau::nsize_t getElementCount() const noexcept override {
                return getPDUValueSize()  / getElementTotalSize();
            }

            Element getElement(const jau::nsize_t elementIdx) const {
                return Element(*this, elementIdx);
            }

            uint16_t getElementHandle(const jau::nsize_t elementIdx) const {
                return pdu.get_uint16( getElementPDUOffset(elementIdx) );
            }

            uint8_t * getElementValuePtr(const jau::nsize_t elementIdx) {
                return pdu.get_wptr() + getElementPDUOffset(elementIdx) + 2 /* handle size */;
            }

            std::string getName() const noexcept override {
                return "AttReadByTypeRsp";
            }

        protected:
            std::string elementString(const jau::nsize_t idx) const override {
                return getElement(idx).toString();
            }
    };


    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.4.10
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
                    const TOctetSlice view;

                public:
                    Element(const AttReadByGroupTypeRsp & p, const jau::nsize_t idx)
                    : view(p.pdu, p.getElementPDUOffset(idx), p.getElementTotalSize()) {}

                    uint16_t getStartHandle() const noexcept {
                        return view.get_uint16_nc(0);
                    }

                    uint16_t getEndHandle() const noexcept {
                        return view.get_uint16_nc(2);
                    }

                    uint8_t const * getValuePtr() const noexcept {
                        return view.get_ptr_nc(4 /* handle size */);
                    }

                    jau::nsize_t getValueSize() const noexcept { return view.getSize() - 4 /* handle size */; }
            };

            AttReadByGroupTypeRsp(const uint8_t* source, const jau::nsize_t length)
            : AttElementList(source, length)
            {
                checkOpcode(Opcode::READ_BY_GROUP_TYPE_RSP);

                if( getPDUValueSize() % getElementTotalSize() != 0 ) {
                    throw AttValueException("PDUReadByGroupTypeRsp: Invalid packet size: pdu-value-size "+std::to_string(getPDUValueSize())+
                            " not multiple of element-size "+std::to_string(getElementTotalSize()), E_FILE_LINE);
                }
            }

            /** opcode + element-size */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 1; }

            /** Returns size of each element, i.e. handle-value triple. */
            jau::nsize_t getElementTotalSize() const noexcept override {
                return pdu.get_uint8_nc(1);
            }

            /**
             * Net element-value size, i.e. element size less handles.
             * <p>
             * element := { uint16_t startHandle, uint16_t endHandle, uint8_t value[value-size] }
             * </p>
             */
            jau::nsize_t getElementValueSize() const noexcept override {
                return getElementTotalSize() - 4;
            }

            jau::nsize_t getElementCount() const noexcept override {
                return getPDUValueSize()  / getElementTotalSize();
            }

            Element getElement(const jau::nsize_t elementIdx) const {
                return Element(*this, elementIdx);
            }

            uint16_t getElementStartHandle(const jau::nsize_t elementIdx) const {
                return pdu.get_uint16( getElementPDUOffset(elementIdx) );
            }

            uint16_t getElementEndHandle(const jau::nsize_t elementIdx) const {
                return pdu.get_uint16( getElementPDUOffset(elementIdx) + 2 /* 1 handle size */ );
            }

            uint8_t * getElementValuePtr(const jau::nsize_t elementIdx) {
                return pdu.get_wptr() + getElementPDUOffset(elementIdx) + 4 /* 2 handle size */;
            }

            std::string getName() const noexcept override {
                return "AttReadByGroupTypeRsp";
            }

        protected:
            std::string elementString(const jau::nsize_t idx) const override {
                Element e = getElement(idx);
                return "handle ["+jau::uint16HexString(e.getStartHandle(), true)+".."+jau::uint16HexString(e.getEndHandle(), true)+
                       "], data "+jau::bytesHexString(e.getValuePtr(), 0, e.getValueSize(), true /* lsbFirst */, true /* leading0X */);
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.3.1
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
            AttFindInfoReq(const uint16_t startHandle, const uint16_t endHandle)
            : AttPDUMsg(Opcode::FIND_INFORMATION_REQ, 1+2+2)
            {
                pdu.put_uint16_nc(1, startHandle);
                pdu.put_uint16_nc(3, endHandle);
            }

            /** opcode + handle_start + handle_end */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 2 + 2; }

            uint16_t getStartHandle() const noexcept {
                return pdu.get_uint16_nc( 1 );
            }

            uint16_t getEndHandle() const noexcept {
                return pdu.get_uint16_nc( 1 + 2 );
            }

            std::string getName() const noexcept override {
                return "AttFindInfoReq";
            }

        protected:
            std::string valueString() const noexcept override {
                return "handle ["+jau::uint16HexString(getStartHandle(), true)+".."+jau::uint16HexString(getEndHandle(), true)+"]";
            }
    };

    /**
     * ATT Protocol PDUs Vol 3, Part F 3.4.3.2
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
            uuid_t::TypeSize getUUIFormat() const {
                const int f = pdu.get_uint8_nc(1);
                if( 0x01 == f ) {
                    return uuid_t::TypeSize::UUID16_SZ;
                } else if( 0x02 == f ) {
                    return uuid_t::TypeSize::UUID128_SZ;
                }
                throw AttValueException("PDUFindInfoRsp: Invalid format "+std::to_string(f)+", not UUID16 (1) or UUID128 (2)", E_FILE_LINE);
            }

        public:
            /**
             * element := { uint16_t handle, UUID value }, with a UUID of UUID16 or UUID128
             */
            class Element {
                public:
                    const uint16_t handle;
                    const std::unique_ptr<const uuid_t> uuid;

                    Element(const AttFindInfoRsp & p, const jau::nsize_t idx)
                    : handle( p.getElementHandle(idx) ), uuid( p.getElementValue(idx) )
                    { }
            };

            AttFindInfoRsp(const uint8_t* source, const jau::nsize_t length) : AttElementList(source, length) {
                checkOpcode(Opcode::FIND_INFORMATION_RSP);
                if( getPDUValueSize() % getElementTotalSize() != 0 ) {
                    throw AttValueException("PDUFindInfoRsp: Invalid packet size: pdu-value-size "+std::to_string(getPDUValueSize())+
                            " not multiple of element-size "+std::to_string(getElementTotalSize()), E_FILE_LINE);
                }
            }

            /** opcode + format */
            jau::nsize_t getPDUValueOffset() const noexcept override { return 1 + 1; }

            /** Returns size of each element, i.e. handle-value tuple. */
            jau::nsize_t getElementTotalSize() const override {
                return 2 + getElementValueSize();
            }

            /**
             * Net element-value size, i.e. element size less handles.
             * <p>
             * element := { uint16_t handle, UUID value }, with a UUID of UUID16 or UUID128
             * </p>
             */
            jau::nsize_t getElementValueSize() const override {
                return uuid_t::number(getUUIFormat());
            }

            jau::nsize_t getElementCount() const override {
                return getPDUValueSize()  / getElementTotalSize();
            }

            Element getElement(const jau::nsize_t elementIdx) const {
                return Element(*this, elementIdx);
            }

            uint16_t getElementHandle(const jau::nsize_t elementIdx) const {
                return pdu.get_uint16( getElementPDUOffset(elementIdx) );
            }

            std::unique_ptr<const uuid_t> getElementValue(const jau::nsize_t elementIdx) const {
                return pdu.get_uuid( getElementPDUOffset(elementIdx) + 2, getUUIFormat() );
            }

            std::string getName() const noexcept override {
                return "AttFindInfoRsp";
            }

        protected:
            std::string addValueString() const override { return "format "+std::to_string(pdu.get_uint8_nc(1))+", "; }

            std::string elementString(const jau::nsize_t idx) const override {
                Element e = getElement(idx);
                return "handle "+jau::uint16HexString(e.handle, true)+
                       ", uuid "+e.uuid.get()->toString();
            }
    };
}

/** \example dbt_scanner10.cpp
 * This C++ scanner example uses the Direct-BT fully event driven workflow
 * and adds multithreading, i.e. one thread processes each found device found
 * as notified via the event listener.
 * <p>
 * This example represents the recommended utilization of Direct-BT.
 * </p>
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
