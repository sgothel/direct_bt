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

#ifndef SMP_TYPES_HPP_
#define SMP_TYPES_HPP_

#include <cstring>
#include <string>
#include <cstdint>

#include <algorithm>
#include <mutex>

#include <jau/basic_types.hpp>

#include "OctetTypes.hpp"

/**
 * - - - - - - - - - - - - - - -
 *
 * SMPTypes.hpp Module for SMPPDUMsg Types, SMPAuthReqs etc:
 *
 * - BT Core Spec v5.2: Vol 3, Part H Security Manager Specification (SM): 2 Security Manager (SM)
 * - BT Core Spec v5.2: Vol 3, Part H Security Manager Specification (SM): 3 Security Manager Protocol (SMP)
 *
 */
namespace direct_bt {

    class SMPException : public jau::RuntimeException {
        protected:
            SMPException(std::string const type, std::string const m, const char* file, int line) noexcept
            : RuntimeException(type, m, file, line) {}

        public:
            SMPException(std::string const m, const char* file, int line) noexcept
            : RuntimeException("HCIException", m, file, line) {}
    };

    class SMPPacketException : public SMPException {
        public:
            SMPPacketException(std::string const m, const char* file, int line) noexcept
            : SMPException("SMPPacketException", m, file, line) {}
    };
    class SMPOpcodeException : public SMPException {
        public:
            SMPOpcodeException(std::string const m, const char* file, int line) noexcept
            : SMPException("SMPOpcodeException", m, file, line) {}
    };

    class SMPValueException : public SMPException {
        public:
            SMPValueException(std::string const m, const char* file, int line) noexcept
            : SMPException("SMPValueException", m, file, line) {}
    };

    enum class SMPConstInt : int32_t {
    };
    constexpr int32_t number(const SMPConstInt rhs) noexcept {
        return static_cast<int>(rhs);
    }

    enum class SMPConstU16 : uint16_t {
        /** SMP Timeout Vol 3, Part H (SM): 3.4 */
        SMP_TIMEOUT_MS        = 30000
    };
    constexpr uint16_t number(const SMPConstU16 rhs) noexcept {
        return static_cast<uint16_t>(rhs);
    }


    /**
     * SMP Authentication Requirements Bits, denotes specific bits or whole protocol uint8_t bit-mask.
     * <pre>
     * SMP Pairing Request Vol 3, Part H (SM): 3.5.1
     * SMP Pairing Response Vol 3, Part H (SM): 3.5.2
     * SMP Security Request Vol 3, Part H (SM): 3.6.7
     * </pre>
     *
     * Layout LSB -> MSB
     * <pre>
     * uint8_t bonding_flags : 2, mitm : 1, sc : 1, keypress : 1, ct2 : 1, rfu : 2;
     * </pre>
     */
    enum class SMPAuthReqs : uint8_t {
        NONE                        =          0,
        /**
         * Indicate bonding being requested by the initiating device.
         */
        BONDING                     = 0b00000001,
        /** Reserved for future use */
        BONDING_RESERVED            = 0b00000010,
        /**
         * A device sets the MITM flag to one to request an Authenticated security property
         * for the STK when using LE legacy pairing
         * and the LTK when using LE Secure Connections.
         */
        MITM                        = 0b00000100,
        /**
         * If LE Secure Connections pairing is supported by
         * the device, then the SC field shall be set to 1, otherwise it shall be set to 0.
         * If both devices support LE Secure Connections pairing,
         * then LE Secure Connections pairing shall be used,
         * otherwise LE Legacy pairing shall be used.
         */
        LE_SECURE_CONNECTIONS       = 0b00001000,
        /**
         * The keypress field is used only in the Passkey Entry
         * protocol and shall be ignored in other protocols.
         * When both sides set that field to one,
         * Keypress notifications shall be generated and sent using SMP
         * Pairing Keypress Notification PDUs.
         */
        KEYPRESS                    = 0b00010000,
        /**
         * The CT2 field shall be set to 1 upon transmission to indicate
         * support for the h7 function.
         * <pre>
         * See sections:
         * - 2.4.2.4 Derivation of BR/EDR link key from LE LTK
         * - 2.4.2.5 Derivation of LE LTK from BR/EDR link key
         * </pre>
         */
        CT2_H7_FUNC_SUPPORT         = 0b00100000,
        /** Reserved for future use */
        RFU_1                       = 0b01000000,
        /** Reserved for future use */
        RFU_2                       = 0b10000000
    };
    constexpr uint8_t number(const SMPAuthReqs rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    constexpr bool isSMPAuthReqBitSet(const SMPAuthReqs mask, const SMPAuthReqs bit) noexcept {
        return 0 != ( static_cast<uint8_t>(mask) & static_cast<uint8_t>(bit) );
    }
    std::string getSMPAuthReqBitString(const SMPAuthReqs bit) noexcept;
    std::string getSMPAuthReqMaskString(const SMPAuthReqs mask) noexcept;

    /**
     * Handles the Security Manager Protocol (SMP) using Protocol Data Unit (PDU)
     * encoded messages over L2CAP channel.
     * <p>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.3 Command Format
     * </p>
     * <p>
     * Message format between both devices, negotiating security details.
     * </p>
     * <p>
     * Vol 3 (Host), Part H Security Manager Specification (SM): 1.2.1 Bit and byte ordering conventions<br>
     * Little-Endian: Multiple-octet fields shall be transmitted with the least significant octet first.
     * </p>
     * @see SMPAuthReqs
     */
    class SMPPDUMsg
    {
        public:
            /** SMP Command Codes Vol 3, Part H (SM): 3.3 */
            enum class Opcode : uint8_t {
                UNDEFINED                       = 0x00, // our own pseudo opcode, indicating no ATT PDU message

                PAIRING_REQUEST                 = 0x01,
                PAIRING_RESPONSE                = 0x02,
                PAIRING_CONFIRM                 = 0x03,
                PAIRING_RANDOM                  = 0x04,
                PAIRING_FAILED                  = 0x05,

                ENCRYPTION_INFORMATION          = 0x06,
                MASTER_IDENTIFICATION           = 0x07,
                IDENTITY_INFORMATION            = 0x08,
                IDENTITY_ADDRESS_INFORMATION    = 0x09,
                SIGNING_INFORMATION             = 0x0A,
                SECURITY_REQUEST                = 0x0B,

                PAIRING_PUBLIC_KEY              = 0x0C,
                PAIRING_DHKEY_CHECK             = 0x0D,
                PAIRING_KEYPRESS_NOTIFICATION   = 0x0E
            };
            static constexpr uint8_t number(const Opcode rhs) noexcept {
                return static_cast<uint8_t>(rhs);
            }
            static std::string getOpcodeString(const Opcode opc) noexcept;

        protected:
            void checkOpcode(const Opcode expected) const
            {
                const Opcode has = getOpcode();
                if( expected != has ) {
                    throw SMPOpcodeException("Has opcode "+jau::uint8HexString(number(has), true)+" "+getOpcodeString(has)+
                                     ", but expected "+jau::uint8HexString(number(expected), true)+" "+getOpcodeString(expected), E_FILE_LINE);
                }
            }
            void checkOpcode(const Opcode exp1, const Opcode exp2) const
            {
                const Opcode has = getOpcode();
                if( exp1 != has && exp2 != has ) {
                    throw SMPOpcodeException("Has opcode "+jau::uint8HexString(number(has), true)+" "+getOpcodeString(has)+
                                     ", but expected either "+jau::uint8HexString(number(exp1), true)+" "+getOpcodeString(exp1)+
                                     " or  "+jau::uint8HexString(number(exp1), true)+" "+getOpcodeString(exp1), E_FILE_LINE);
                }
            }

            virtual std::string baseString() const noexcept {
                return "opcode="+jau::uint8HexString(number(getOpcode()), true)+" "+getOpcodeString()+
                        ", size[total="+std::to_string(pdu.getSize())+", param "+std::to_string(getPDUParamSize())+"]";
            }
            virtual std::string valueString() const noexcept {
                return "size "+std::to_string(getDataSize())+", data "
                        +jau::bytesHexString(pdu.get_ptr(), getDataOffset(), getDataSize(), true /* lsbFirst */, true /* leading0X */);
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
            static std::shared_ptr<const SMPPDUMsg> getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept;

            /** Persistent memory, w/ ownership ..*/
            SMPPDUMsg(const uint8_t* source, const jau::nsize_t size)
                : pdu(source, std::max<jau::nsize_t>(1, size)), ts_creation(jau::getCurrentMilliseconds())
            {
                pdu.check_range(0, getDataOffset()+getDataSize());
            }

            /** Persistent memory, w/ ownership ..*/
            SMPPDUMsg(const Opcode opc, const jau::nsize_t size)
                : pdu(std::max<jau::nsize_t>(1, size)), ts_creation(jau::getCurrentMilliseconds())
            {
                pdu.put_uint8_nc(0, number(opc));
                pdu.check_range(0, getDataOffset()+getDataSize());
            }

            SMPPDUMsg(const SMPPDUMsg &o) noexcept = default;
            SMPPDUMsg(SMPPDUMsg &&o) noexcept = default;
            SMPPDUMsg& operator=(const SMPPDUMsg &o) noexcept = delete; // const ts_creation
            SMPPDUMsg& operator=(SMPPDUMsg &&o) noexcept = delete; // const ts_creation

            virtual ~SMPPDUMsg() noexcept {}

            /** SMP Command Codes Vol 3, Part H (SM): 3.3 */
            inline Opcode getOpcode() const noexcept {
                return static_cast<Opcode>(pdu.get_uint8_nc(0));
            }
            std::string getOpcodeString() const noexcept { return getOpcodeString(getOpcode()); }

            /**
             * Returns the actual PDU size less one octet for the opcode,
             * which should result in 0-22 octets or 64 octets.
             * <p>
             * Note that the PDU parameter include the data value below.
             * </p>
             * <p>
             * Use getDataSize() for the actual required data size
             * according to the specific packet.
             * </p>
             *
             * @see SMPPDUMsg::getDataSize()
             */
            jau::nsize_t getPDUParamSize() const noexcept {
                return pdu.getSize() - 1 /* opcode */;
            }

            /**
             * Returns the required data size according to the specified packet,
             * which should be within 0-22 or 64 octets.
             *
             * @see SMPPDUMsg::getPDUParamSize()
             */
            virtual jau::nsize_t getDataSize() const noexcept {
                return getPDUParamSize();
            }


            /**
             * Returns the octet offset to the data segment in this PDU
             * including the mandatory opcode,
             * i.e. the number of octets until the first value octet.
             */
            constexpr jau::nsize_t getDataOffset() const noexcept { return 1; /* default: opcode */ }

            virtual std::string getName() const noexcept {
                return "SMPPDUMsg";
            }

            virtual std::string toString() const noexcept{
                return getName()+"["+baseString()+", value["+valueString()+"]]";
            }
    };

    /**
     * Vol 3, Part H: 3.5.1 Pairing Request message.<br>
     * Vol 3, Part H: 3.5.2 Pairing Response message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.5 Pairing Methods
     * Vol 3 (Host), Part H (SM): 2 (SM), 2.3 Pairing Methods
     * </pre>
     *
     * Opcode::PAIRING_REQUEST or Opcode::PAIRING_RESPONSE
     *
     * <pre>
     * [uint8_t opcode]
     * uint8_t io_capability
     * uint8_t oob_data_flag
     * uint8_t auth_req_mask
     * uint8_t max_encryption_key_size
     * uint8_t initiator_key_distribution
     * uint8_t responder_key_distribution
     * </pre>
     *
     * <br>
     * SMP Pairing Request Vol 3, Part H (SM): 3.5.1
     * <p>
     * Initiator starts the Pairing Feature Exchange
     * by sending a Pairing Request command to the responding device.
     * </p>
     * <p>
     * The rules for handing a collision between a pairing procedure
     * on the LE transport and a pairing procedure on the BR/EDR transport
     * are defined in Vol 3, Part C (GAP): 14.2 BRD/EDR/LE security aspects - Collision Handling.
     * </p>
     *
     * <br>
     * SMP Pairing Response Vol 3, Part H (SM): 3.5.2
     * <p>
     * Command is used by the responding device to complete the Pairing Feature Exchange
     * after it has received a Pairing Request command from the initiating device,
     * if the responding device allows pairing.
     * </p>
     * <p>
     * If a Pairing Request is received over the BR/EDR transport
     * when either cross-transport key derivation/generation is not supported
     * or the BR/EDR transport is not encrypted using a Link Key generated using P256,
     * a Pairing Failed shall be sent with the error code
     * SMPPairFailedMsg::ReasonCode::CROSSXPORT_KEY_DERIGEN_NOT_ALLOWED (Cross-Transport Key Derivation/Generation Not Allowed).
     * </p>
     * <p>
     * The rules for handing a collision between a pairing procedure
     * on the LE transport and a pairing procedure on the BR/EDR transport
     * are defined in Vol 3, Part C (GAP): 14.2 BRD/EDR/LE security aspects - Collision Handling.
     * </p>
     */
    class SMPPairingMsg : public SMPPDUMsg
    {
        public:
            /**
             * Vol 3, Part H, 2.3.2 IO capabilities
             */
            enum class IOCapability : uint8_t {
                DISPLAY_ONLY                = 0x00,/**< DISPLAY_ONLY */
                DISPLAY_YES_NO              = 0x01,/**< DISPLAY_YES_NO */
                KEYBOARD_ONLY               = 0x02,/**< KEYBOARD_ONLY */
                NO_INPUT_NO_OUTPUT          = 0x03,/**< NO_INPUT_NO_OUTPUT */
                KEYBOARD_DISPLAY            = 0x04 /**< KEYBOARD_DISPLAY */
            };
            static constexpr uint8_t number(const IOCapability rhs) noexcept {
                return static_cast<uint8_t>(rhs);
            }
            static std::string getIOCapabilityString(const IOCapability ioc) noexcept;

            /**
             * Vol 3, Part H, 2.3.3 OOB authentication data
             *
             */
            enum class OOBDataFlag : uint8_t {
                OOB_AUTH_DATA_NOT_PRESENT    = 0x00,/**< OOB_AUTH_DATA_NOT_PRESENT */
                OOB_AUTH_DATA_REMOTE_PRESENT = 0x01 /**< OOB_AUTH_DATA_REMOTE_PRESENT */
            };
            static constexpr uint8_t number(const OOBDataFlag rhs) noexcept {
                return static_cast<uint8_t>(rhs);
            }
            static std::string getOOBDataFlagString(const OOBDataFlag v) noexcept;

            /**
             * LE Key Distribution format, indicates keys distributed in the Transport Specific Key Distribution phase.
             * <pre>
             * Field format and usage: Vol 3, Part H, 3.6.1 SMP - LE Security - Key distribution and generation.
             * See also Vol 3, Part H, 2.4.3 SM - LE Security - Distribution of keys.
             * </pre>
             * </p>
             * Layout LSB -> MSB
             * <pre>
             * uint8_t EncKey : 1, IdKey : 1, SignKey : 1, LinkKey : 1, RFU : 4;
             * </pre>
             */
            enum class KeyDistFormat : uint8_t {
                NONE                        =          0,
                /**
                 * LE legacy pairing: Indicates device shall distribute LTK using the Encryption Information command,
                 * followed by EDIV and Rand using the Master Identification command.
                 * <p>
                 * LE Secure Connections pairing (SMP on LE transport): Ignored,
                 * EDIV and Rand shall be zero and shall not be distributed.
                 * </p>
                 * <p>
                 * SMP on BR/EDR transport: Indicates device likes to derive LTK from BR/EDR Link Key.<br>
                 * When EncKey is set to 1 by both devices in the initiator and responder Key Distribution / Generation fields,
                 * the procedures for calculating the LTK from the BR/EDR Link Key shall be used.
                 * </p>
                 */
                ENC_KEY                     = 0b00000001,
                /**
                 * Indicates that the device shall distribute IRK using the Identity Information command
                 * followed by its public device or statuc random address using Identity Address Information.
                 */
                ID_KEY                      = 0b00000010,
                /**
                 * Indicates that the device shall distribute CSRK using the Signing Information command.
                 */
                SIGN_KEY                    = 0b00000100,
                /**
                 * SMP on the LE transport: Indicate that the device would like to derive the Link Key from the LTK.<br>
                 * When LinkKey is set to 1 by both devices in the initiator and responder Key Distribution / Generation fields,
                 * the procedures for calculating the BR/EDR link key from the LTK shall be used.<br>
                 * Devices not supporting LE Secure Connections shall set this bit to zero and ignore it on reception.
                 * <p>
                 * SMP on BR/EDR transport: Reserved for future use.
                 * </p>
                 */
                LINK_KEY                    = 0b00001000,
                /** Reserved for future use */
                RFU_1                       = 0b00010000,
                /** Reserved for future use */
                RFU_2                       = 0b00100000,
                /** Reserved for future use */
                RFU_3                       = 0b01000000,
                /** Reserved for future use */
                RFU_4                       = 0b10000000
            };
            static constexpr uint8_t number(const KeyDistFormat rhs) noexcept {
                return static_cast<uint8_t>(rhs);
            }
            static constexpr bool isKeyDistFormatBitSet(const KeyDistFormat mask, const KeyDistFormat bit) noexcept {
                return 0 != ( static_cast<uint8_t>(mask) & static_cast<uint8_t>(bit) );
            }
            static std::string getKeyDistFormatBitString(const KeyDistFormat bit) noexcept;
            static std::string getKeyDistFormatMaskString(const KeyDistFormat mask) noexcept;

        private:
            const bool request;
            const SMPAuthReqs authReqMask;
            const KeyDistFormat initiator_key_dist;
            const KeyDistFormat responder_key_dist;

        public:
            SMPPairingMsg(const bool request_, const uint8_t* source, const jau::nsize_t length)
            : SMPPDUMsg(source, length),
              request(request_),
              authReqMask(static_cast<SMPAuthReqs>( pdu.get_uint8_nc(3) )),
              initiator_key_dist(static_cast<KeyDistFormat>(pdu.get_uint8_nc(5))),
              responder_key_dist(static_cast<KeyDistFormat>(pdu.get_uint8_nc(6)))
            {
                checkOpcode(request? Opcode::PAIRING_REQUEST : Opcode::PAIRING_RESPONSE);
            }

            SMPPairingMsg(const bool request_,
                          const IOCapability ioc, const OOBDataFlag odf,
                          const SMPAuthReqs auth_req_mask, const uint8_t maxEncKeySize,
                          const KeyDistFormat initiator_key_dist_,
                          const KeyDistFormat responder_key_dist_)
            : SMPPDUMsg(request_? Opcode::PAIRING_REQUEST : Opcode::PAIRING_RESPONSE, 1+6),
              request(request_),
              authReqMask(auth_req_mask), initiator_key_dist(initiator_key_dist_), responder_key_dist(responder_key_dist_)
            {
                pdu.put_uint8_nc(1, number(ioc));
                pdu.put_uint8_nc(2, number(odf));
                pdu.put_uint8_nc(3, direct_bt::number(authReqMask));
                pdu.put_uint8_nc(4, maxEncKeySize);
                pdu.put_uint8_nc(5, number(initiator_key_dist));
                pdu.put_uint8_nc(6, number(responder_key_dist));
            }

            jau::nsize_t getDataSize() const noexcept override {
                return 6;
            }

            /**
             * Returns the IO capability bit field.
             * <pre>
             * Vol 3, Part H, 2.3.2 IO capabilities
             * </pre>
             * @see IOCapability
             */
            IOCapability getIOCapability() const noexcept {
                return static_cast<IOCapability>(pdu.get_uint8_nc(1));
            }

            /**
             * Returns the OBB authenticate data flag
             * <pre>
             * Vol 3, Part H, 2.3.3 OOB authentication data
             * </pre>
             * @see OOBDataFlag
             */
            OOBDataFlag getOOBDataFlag() const noexcept {
                return static_cast<OOBDataFlag>(pdu.get_uint8_nc(2));
            }

            /**
             * Returns the Authentication Requirements mask.
             * <pre>
             * SMP Pairing Request Vol 3, Part H (SM): 3.5.1
             * SMP Pairing Response Vol 3, Part H (SM): 3.5.2
             * </pre>
             * @see AuthRequirements
             */
            constexpr SMPAuthReqs getAuthReqMask() const noexcept {
                return authReqMask;
            }
            constexpr bool isAuthRequirementBitSet(const SMPAuthReqs bit) const noexcept {
                return isSMPAuthReqBitSet(authReqMask, bit);
            }

            /**
             * This value defines the maximum encryption key size in octets that the device
             * can support. The maximum key size shall be in the range 7 to 16 octets.
             */
            uint8_t getMaxEncryptionKeySize() const noexcept {
                return pdu.get_uint8_nc(4);
            }
            /**
             * Returns the Initiator Key Distribution field,
             * which defines which keys the initiator shall
             * distribute and use during the Transport Specific Key Distribution phase.
             * <pre>
             * See Vol 3, Part H, 2.4.3 SM - LE Security - Distribution of keys.
             * Field format and usage: Vol 3, Part H, 3.6.1 SMP - LE Security - Key distribution and generation.
             * </pre>
             * @see KeyDistributionFormat
             */
            constexpr KeyDistFormat getInitiatorKeyDistribution() const noexcept {
                return initiator_key_dist;
            }
            constexpr bool isInitiatorKeyDistributionBitSet(const KeyDistFormat bit) const noexcept {
                return isKeyDistFormatBitSet(initiator_key_dist, bit);
            }
            /**
             * Return the Responder Key Distribution field,
             * which defines which keys the responder shall
             * distribute and use during the Transport Specific Key Distribution phase.
             * <p>
             * See Vol 3, Part H, 2.4.3 SM - LE Security - Distribution of keys.
             * Field format and usage: Vol 3, Part H, 3.6.1 SMP - LE Security - Key distribution and generation.
             * </p>
             * @see KeyDistributionFormat
             */
            constexpr KeyDistFormat getResponderKeyDistribution() const noexcept {
                return responder_key_dist;
            }
            constexpr bool isResponderKeyDistributionBitSet(const KeyDistFormat bit) const noexcept {
                return isKeyDistFormatBitSet(initiator_key_dist, bit);
            }

            std::string getName() const noexcept override {
                return request ? "SMPPairReq" : "SMPPairRes";
            }

        protected:
            std::string valueString() const noexcept override {
                return "iocap "+getIOCapabilityString(getIOCapability())+
                       ", oob "+getOOBDataFlagString(getOOBDataFlag())+
                       ", auth_req "+getSMPAuthReqMaskString(getAuthReqMask())+
                       ", max_keysz "+std::to_string(getMaxEncryptionKeySize())+
                       ", key_dist[init "+getKeyDistFormatMaskString(getInitiatorKeyDistribution())+
                       ", resp "+getKeyDistFormatMaskString(getResponderKeyDistribution())+
                       "]";
            }
    };

    /**
     * Vol 3, Part H: 3.5.3 Pairing Confirm message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.5 Pairing Methods
     * </pre>
     *
     * Opcode::PAIRING_CONFIRM
     *
     * <pre>
     * [uint8_t opcode]
     * jau::uint128_t confirm_value
     * </pre>
     *
     * Used following a successful Pairing Feature Exchange to start
     * STK Generation for LE legacy pairing and LTK Generation for LE Secure Connections pairing.
     * <p>
     * Command is used by both devices to send the confirm value to the peer device,<br>
     * see Vol 3, Part H, 2.3.5.5 SM - Pairing algo - LE legacy pairing phase 2 and<br>
     * and Vol 3, Part H, 2.3.5.6 SM - Pairing algo - LE Secure Connections pairing phase 2.
     * </p>
     * <p>
     * The initiating device starts key generation by sending the Pairing Confirm command to the responding device.<br>
     * If the initiating device wants to abort pairing it can transmit a Pairing Failed command instead.
     * </p>
     * <p>
     * The responding device sends the Pairing Confirm command
     * after it has received a Pairing Confirm command from the initiating device.
     * </p>
     */
    class SMPPairConfMsg : public SMPPDUMsg
    {
        public:
            SMPPairConfMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPPDUMsg(source, length)
            {
                checkOpcode(Opcode::PAIRING_CONFIRM);
            }

            SMPPairConfMsg(const jau::uint128_t & confirm_value)
            : SMPPDUMsg(Opcode::PAIRING_CONFIRM, 1+16)
            {
                pdu.put_uint128_nc(1, confirm_value);
            }

            jau::nsize_t getDataSize() const noexcept override {
                return 16;
            }

            /**
             * Returns the 128-bit Confirm value (16 octets)
             * <p>
             * In LE legacy pairing, the initiating device sends Mconfirm and the
             * responding device sends Sconfirm as defined in
             * Vol 3, Part H, 2.3.5.5 SM - Pairing algo - LE legacy pairing phase 2
             * </p>
             * <p>
             * In LE Secure Connections, Ca and Cb are defined in
             * Vol 3, Part H, Section 2.2.6 SM - Crypto Toolbox - LE Secure Connections confirm value generation function f4.<br>
             * See Vol 3, Part H, 2.3.5.6 SM - Pairing algo - LE Secure Connections pairing phase 2.
             * </p>
             */
            jau::uint128_t getConfirmValuePtr() const noexcept { return pdu.get_uint128_nc(1); }

            std::string getName() const noexcept override {
                return "SMPPairConf";
            }

        protected:
            std::string valueString() const noexcept override {
                return "size "+std::to_string(getDataSize())+", data anon"; // FIXME: Shareable?
            }
    };

    /**
     * Vol 3, Part H: 3.5.4 Pairing Random message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.5 Pairing Methods
     * </pre>
     *
     * Opcode::PAIRING_RANDOM
     *
     * <pre>
     * [uint8_t opcode]
     * jau::uint128_t random_value
     * </pre>
     *
     * Used by the initiating and responding device to send the
     * random number used to calculate the Confirm value sent in the Pairing Confirm command.
     * <p>
     * The initiating device sends a Pairing Random command
     * after it has received a Pairing Confirm command from the responding device.
     * </p>
     * <p>
     * LE legacy pairing: Responding device sends a Pairing Random command
     * after receiving a Pairing Random command from the initiating device
     * if the Confirm value calculated on the responding device matches the
     * Confirm value received from the initiating device.<br>
     * If the calculated Confirm value does not match
     * then the responding device shall respond with the Pairing Failed command.
     * </p>
     * <p>
     * LE Secure Connections: Responding device sends a Pairing Random command
     * after it has received a Pairing Random command from the initiating device
     * <i>if the Confirm value calculated on the responding device matches the
     * Confirm value received from the initiating device(?)</i>.<br>
     * If the calculated Confirm value does not match then the responding device
     * shall respond with the Pairing Failed command.
     * </p>
     * <p>
     * The initiating device shall encrypt the link using
     * the generated key (STK in LE legacy pairing or LTK in LE Secure Connections)
     * if the Confirm value calculated on the initiating device matches
     * the Confirm value received from the responding device.<br>
     * The successful encryption or re-encryption of the link is the signal
     * to the responding device that key generation has completed successfully.<br>
     * If the calculated Confirm value does not match then
     * the initiating device shall respond with the Pairing Failed command.
     * </p>
     */
    class SMPPairRandMsg : public SMPPDUMsg
    {
        public:
            SMPPairRandMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPPDUMsg(source, length)
            {
                checkOpcode(Opcode::PAIRING_RANDOM);
            }

            SMPPairRandMsg(const jau::uint128_t & random_value)
            : SMPPDUMsg(Opcode::PAIRING_RANDOM, 1+16)
            {
                pdu.put_uint128_nc(1, random_value);
            }

            jau::nsize_t getDataSize() const noexcept override {
                return 16;
            }

            /**
             * Returns the 128-bit Random value (16 octets)
             * <p>
             * In LE legacy pairing, the initiating device sends Mrand and the
             * responding device sends Srand as defined in
             * Vol 3, Part H, 2.3.5.5 SM - Pairing algo - LE legacy pairing phase 2
             * </p>
             * <p>
             * In LE Secure Connections,
             * the initiating device sends Na and the responding device sends Nb.
             * </p>
             */
            jau::uint128_t getRandomValuePtr() const noexcept { return pdu.get_uint128_nc(1); }

            std::string getName() const noexcept override {
                return "SMPPairRand";
            }

        protected:
            std::string valueString() const noexcept override {
                return "size "+std::to_string(getDataSize())+", data anon";
            }
    };

    /**
     * Vol 3, Part H: 3.5.5 Pairing Failed message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.5 Pairing Methods
     * </pre>
     *
     * Opcode::PAIRING_FAILED
     *
     * <pre>
     * [uint8_t opcode]
     * uint8_t reason_code
     * </pre>
     */
    class SMPPairFailedMsg: public SMPPDUMsg
    {
        public:
            enum class ReasonCode : uint8_t {
                UNDEFINED                   = 0x00,
                PASSKEY_ENTRY_FAILED        = 0x01,
                OOB_NOT_AVAILABLE           = 0x02,
                AUTHENTICATION_REQUIREMENTS = 0x03,
                CONFIRM_VALUE_FAILED        = 0x04,
                PAIRING_NOT_SUPPORTED       = 0x05,
                ENCRYPTION_KEY_SIZE         = 0x06,
                COMMON_NOT_SUPPORTED        = 0x07,
                UNSPECIFIED_REASON          = 0x08,
                REPEATED_ATTEMPTS           = 0x09,
                INVALID_PARAMTERS           = 0x0A,
                DHKEY_CHECK_FAILED          = 0x0B,
                NUMERIC_COMPARISON_FAILED   = 0x0C,
                BREDR_PAIRING_IN_PROGRESS   = 0x0D,
                CROSSXPORT_KEY_DERIGEN_NOT_ALLOWED = 0x0E
            };
            static constexpr uint8_t number(const ReasonCode rhs) noexcept {
                return static_cast<uint8_t>(rhs);
            }
            static std::string getPlainReasonString(const ReasonCode reasonCode) noexcept;

            SMPPairFailedMsg(const uint8_t* source, const jau::nsize_t length) : SMPPDUMsg(source, length) {
                checkOpcode(Opcode::PAIRING_FAILED);
            }

            SMPPairFailedMsg(const ReasonCode rc)
            : SMPPDUMsg(Opcode::PAIRING_FAILED, 1+1)
            {
                pdu.put_uint8_nc(1, number(rc));
            }

            jau::nsize_t getDataSize() const noexcept override {
                return 1;
            }

            ReasonCode getReasonCode() const noexcept {
                return static_cast<ReasonCode>(pdu.get_uint8_nc(1));
            }

            std::string getName() const noexcept override {
                return "SMPPairFailed";
            }

        protected:
            std::string valueString() const noexcept override {
                const ReasonCode ec = getReasonCode();
                return jau::uint8HexString(number(ec), true) + ": " + getPlainReasonString(ec);
            }
    };


    /**
     * Vol 3, Part H: 3.5.6 Pairing Public Key message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.5 Pairing Methods
     * </pre>
     *
     * Opcode::PAIRING_PUBLIC_KEY
     *
     * <pre>
     * [uint8_t opcode]
     * jau::uint256_t public_key_x_value
     * jau::uint256_t public_key_y_value
     * </pre>
     *
     * Message is used to transfer the device’s local public key (X and Y coordinates) to the remote device.<br>
     * This message is used by both the initiator and responder.<br>
     * This PDU is only used for LE Secure Connections.
     */
    class SMPPairPubKeyMsg : public SMPPDUMsg
    {
        public:
            SMPPairPubKeyMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPPDUMsg(source, length)
            {
                checkOpcode(Opcode::PAIRING_PUBLIC_KEY);
            }

            SMPPairPubKeyMsg(const jau::uint256_t & pub_key_x, const jau::uint256_t & pub_key_y)
            : SMPPDUMsg(Opcode::PAIRING_PUBLIC_KEY, 1+32+32)
            {
                pdu.put_uint256_nc(1, pub_key_x);
                pdu.put_uint256_nc(1+32, pub_key_y);
            }

            jau::nsize_t getDataSize() const noexcept override {
                return 32+32;
            }

            /**
             * Returns the 256-bit Public Key X value (32 octets)
             */
            jau::uint256_t getPublicKeyXValuePtr() const noexcept { return pdu.get_uint256_nc(1); }

            /**
             * Returns the 256-bit Public Key Y value (32 octets)
             */
            jau::uint256_t getPublicKeyYValuePtr() const noexcept { return pdu.get_uint256_nc(1+32); }

            std::string getName() const noexcept override {
                return "SMPPairPubKey";
            }
    };

    /**
     * Vol 3, Part H: 3.5.7 Pairing DHKey Check message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.5 Pairing Methods
     * </pre>
     *
     * Opcode::PAIRING_DHKEY_CHECK
     *
     * <pre>
     * [uint8_t opcode]
     * jau::uint128_t dhkey_check_values
     * </pre>
     *
     * Message is used to transmit the 128-bit DHKey Check values (Ea/Eb) generated using f6.<br>
     * This message is used by both initiator and responder.<br>
     * This PDU is only used for LE Secure Connections.
     */
    class SMPPairDHKeyCheckMsg : public SMPPDUMsg
    {
        public:
            SMPPairDHKeyCheckMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPPDUMsg(source, length)
            {
                checkOpcode(Opcode::PAIRING_DHKEY_CHECK);
            }

            SMPPairDHKeyCheckMsg(const jau::uint128_t & dhkey_check_values)
            : SMPPDUMsg(Opcode::PAIRING_DHKEY_CHECK, 1+16)
            {
                pdu.put_uint128_nc(1, dhkey_check_values);
            }

            jau::nsize_t getDataSize() const noexcept override {
                return 16;
            }

            /**
             * Returns the 128-bit DHKey Check value (16 octets)
             */
            jau::uint128_t getDHKeyCheckValuePtr() const noexcept { return pdu.get_uint128_nc(1); }

            std::string getName() const noexcept override {
                return "SMPPairDHKeyCheck";
            }

        protected:
            std::string valueString() const noexcept override {
                return "size "+std::to_string(getDataSize())+", data anon"; // FIXME: Shareable?
            }
    };

    /**
     * Vol 3, Part H: 3.5.8 Passkey Entry: Keypress notification messages.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.5 Pairing Methods
     * </pre>
     *
     * Opcode::PAIRING_KEYPRESS_NOTIFICATION
     *
     * <pre>
     * [uint8_t opcode]
     * uint8_t notification_type
     * </pre>
     *
     * Message is used during the Passkey Entry protocol by a device with KeyboardOnly IO capabilities
     * to inform the remote device when keys have been entered or erased.
     */
    class SMPPasskeyNotification: public SMPPDUMsg
    {
        public:
            enum class TypeCode : uint8_t {
                PASSKEY_ENTRY_STARTED       = 0x00,
                PASSKEY_DIGIT_ENTERED       = 0x01,
                PASSKEY_DIGIT_ERASED        = 0x02,
                PASSKEY_CLEARED             = 0x03,
                PASSKEY_ENTRY_COMPLETED     = 0x04
            };
            static constexpr uint8_t number(const TypeCode rhs) noexcept {
                return static_cast<uint8_t>(rhs);
            }
            static std::string getTypeCodeString(const TypeCode tc) noexcept;

            SMPPasskeyNotification(const uint8_t* source, const jau::nsize_t length) : SMPPDUMsg(source, length) {
                checkOpcode(Opcode::PAIRING_KEYPRESS_NOTIFICATION);
            }

            SMPPasskeyNotification(const TypeCode tc)
            : SMPPDUMsg(Opcode::PAIRING_KEYPRESS_NOTIFICATION, 1+1)
            {
                pdu.put_uint8_nc(1, number(tc));
            }

            jau::nsize_t getDataSize() const noexcept override {
                return 1;
            }

            TypeCode getTypeCode() const noexcept {
                return static_cast<TypeCode>(pdu.get_uint8_nc(1));
            }

            std::string getName() const noexcept override {
                return "SMPPasskeyNotify";
            }

        protected:
            std::string valueString() const noexcept override {
                const TypeCode ec = getTypeCode();
                return jau::uint8HexString(number(ec), true) + ": " + getTypeCodeString(ec);
            }
    };

    /**
     * Vol 3, Part H: 3.6.2 Encryption Information message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6 SECURITY IN BLUETOOTH LOW ENERGY
     * </pre>
     *
     * Opcode::ENCRYPTION_INFORMATION
     *
     * <pre>
     * [uint8_t opcode]
     * jau::uint128_t long_term_key
     * </pre>
     *
     * Message is used in the LE legacy pairing Transport Specific Key Distribution
     * to distribute LTK that is used when encrypting future connections.
     * <p>
     * The message shall only be sent when the link has been encrypted or re-encrypted using the generated STK.
     * </p>
     */
    class SMPEncInfoMsg : public SMPPDUMsg
    {
        public:
            SMPEncInfoMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPPDUMsg(source, length)
            {
                checkOpcode(Opcode::ENCRYPTION_INFORMATION);
            }

            SMPEncInfoMsg(const jau::uint128_t & long_term_key)
            : SMPPDUMsg(Opcode::ENCRYPTION_INFORMATION, 1+16)
            {
                pdu.put_uint128_nc(1, long_term_key);
            }

            jau::nsize_t getDataSize() const noexcept override {
                return 16;
            }

            /**
             * Returns the 128-bit Long Term Key (16 octets)
             * <p>
             * The generated LTK value being distributed,
             * see Vol 3, Part H, 2.4.2.3 SM - Generation of CSRK - LE legacy pairing - generation of LTK, EDIV and Rand.
             * </p>
             */
            jau::uint128_t getLongTermKeyPtr() const noexcept { return pdu.get_uint128_nc(1); }

            std::string getName() const noexcept override {
                return "SMPEncInfo";
            }

        protected:
            std::string valueString() const noexcept override {
                return "size "+std::to_string(getDataSize())+", data anon";
            }
    };

    /**
     * Vol 3, Part H: 3.6.3 Master Identification message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6 SECURITY IN BLUETOOTH LOW ENERGY
     * </pre>
     *
     * Opcode::MASTER_IDENTIFICATION
     *
     * <pre>
     * [uint8_t opcode]
     * uint16_t ediv
     * uint64_t rand
     * </pre>
     *
     * Message is used in the LE legacy pairing Transport Specific Key Distribution phase
     * to distribute EDIV and Rand which are used when encrypting future connections.
     *
     * <p>
     * The message shall only be sent when the link has been encrypted or re-encrypted using the generated STK.
     * </p>
     */
    class SMPMasterIdentMsg : public SMPPDUMsg
    {
        public:
            SMPMasterIdentMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPPDUMsg(source, length)
            {
                checkOpcode(Opcode::MASTER_IDENTIFICATION);
            }

            SMPMasterIdentMsg(const uint16_t ediv, const uint64_t & rand)
            : SMPPDUMsg(Opcode::MASTER_IDENTIFICATION, 1+2+8)
            {
                pdu.put_uint16_nc(1, ediv);
                pdu.put_uint64_nc(1+2, rand);
            }

            jau::nsize_t getDataSize() const noexcept override {
                return 10;
            }

            /**
             * Returns the 16-bit EDIV value (2 octets) being distributed
             * <p>
             * See Vol 3, Part H, 2.4.2.3 SM - Generation of CSRK - LE legacy pairing - generation of LTK, EDIV and Rand.
             * </p>
             */
            uint16_t getEDIV() const noexcept { return pdu.get_uint16_nc(1); }

            /**
             * Returns the 64-bit Rand value (8 octets) being distributed
             * <p>
             * See Vol 3, Part H, 2.4.2.3 SM - Generation of CSRK - LE legacy pairing - generation of LTK, EDIV and Rand.
             * </p>
             */
            uint64_t getRand() const noexcept { return pdu.get_uint64_nc(1+2); }

            std::string getName() const noexcept override {
                return "SMPMasterIdent";
            }

        protected:
            std::string valueString() const noexcept override {
                return "size "+std::to_string(getDataSize())+", data anon";
            }
    };

    /**
     * Vol 3, Part H: 3.6.4 Identify Information message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6 SECURITY IN BLUETOOTH LOW ENERGY
     * </pre>
     *
     * Opcode::IDENTITY_INFORMATION
     *
     * <pre>
     * [uint8_t opcode]
     * jau::uint128_t identity_resolving_key
     * </pre>
     *
     * Message is used in the Transport Specific Key Distribution phase to distribute IRK.
     * <p>
     * The message shall only shall only be sent when the link has been encrypted or re-encrypted using the generated key.
     * </p>
     */
    class SMPIdentInfoMsg : public SMPPDUMsg
    {
        public:
            SMPIdentInfoMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPPDUMsg(source, length)
            {
                checkOpcode(Opcode::IDENTITY_INFORMATION);
            }

            SMPIdentInfoMsg(const jau::uint128_t & identity_resolving_key)
            : SMPPDUMsg(Opcode::IDENTITY_INFORMATION, 1+16)
            {
                pdu.put_uint128_nc(1, identity_resolving_key);
            }

            jau::nsize_t getDataSize() const noexcept override {
                return 16;
            }

            /**
             * Returns the 128-bit Identity Resolving Key (IRK, 16 octets)
             * <p>
             * The 128-bit IRK value being distributed,
             * see Vol 3, Part H, 2.4.2.1 SM - Definition of keys and values - Generation of IRK.
             * </p>
             */
            jau::uint128_t getIRKPtr() const noexcept { return pdu.get_uint128_nc(1); }

            std::string getName() const noexcept override {
                return "SMPIdentInfo";
            }

        protected:
            std::string valueString() const noexcept override {
                return "size "+std::to_string(getDataSize())+", data anon";
            }
    };


    /**
     * Vol 3, Part H: 3.6.5 Identity Address Information message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6 SECURITY IN BLUETOOTH LOW ENERGY
     * </pre>
     *
     * Opcode::IDENTITY_ADDRESS_INFORMATION
     *
     * <pre>
     * [uint8_t opcode]
     * uint8_t address_type (0x01 static random, 0x00 public)
     * EUI48   address
     * </pre>
     *
     * Message is used in the Transport Specific Key Distribution phase
     * to distribute its public device address or static random address.
     * <p>
     * The message shall only be sent when the link has been encrypted or re-encrypted using the generated key.
     * </p>
     */
    class SMPIdentAddrInfoMsg : public SMPPDUMsg
    {
        public:
            SMPIdentAddrInfoMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPPDUMsg(source, length)
            {
                checkOpcode(Opcode::IDENTITY_ADDRESS_INFORMATION);
            }

            SMPIdentAddrInfoMsg(const bool addrIsStaticRandom, const EUI48 & addr)
            : SMPPDUMsg(Opcode::IDENTITY_ADDRESS_INFORMATION, 1+1+6)
            {
                pdu.put_uint8_nc(1, addrIsStaticRandom ? 0x01 : 0x00);
                pdu.put_eui48_nc(1+1, addr);
            }

            jau::nsize_t getDataSize() const noexcept override {
                return 1+6;
            }

            /**
             * Returns whether the device address is static random (true) or public (false).
             */
            bool isStaticRandomAddress() const noexcept { return pdu.get_uint16_nc(1) == 0x01; }

            /**
             * Returns the device address
             */
            EUI48 getAddress() const noexcept { return pdu.get_eui48_nc(1+1); }

            std::string getName() const noexcept override {
                return "SMPIdentAddrInfo";
            }

        protected:
            std::string valueString() const noexcept override {
                const std::string ats = isStaticRandomAddress() ? "static-random" : "public";
                return "address["+getAddress().toString()+", "+ats+"]";
            }
    };

    /**
     * Vol 3, Part H: 3.6.6 Signing Information message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6 SECURITY IN BLUETOOTH LOW ENERGY
     * </pre>
     *
     * Opcode::SIGNING_INFORMATION
     *
     * <pre>
     * [uint8_t opcode]
     * jau::uint128_t signature_key
     * </pre>
     *
     * Message is used  in the Transport Specific Key Distribution to distribute the CSRK which a device uses to sign data.
     * <p>
     * The message shall only shall only be sent when the link has been encrypted or re-encrypted using the generated key.
     * </p>
     */
    class SMPSignInfoMsg : public SMPPDUMsg
    {
        public:
            SMPSignInfoMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPPDUMsg(source, length)
            {
                checkOpcode(Opcode::SIGNING_INFORMATION);
            }

            SMPSignInfoMsg(const jau::uint128_t & signature_key)
            : SMPPDUMsg(Opcode::SIGNING_INFORMATION, 1+16)
            {
                pdu.put_uint128_nc(1, signature_key);
            }

            jau::nsize_t getDataSize() const noexcept override {
                return 16;
            }

            /**
             * Returns the 128-bit Identity Resolving Key (IRK, 16 octets)
             * <p>
             * The 128-bit IRK value being distributed,
             * see Vol 3, Part H, 2.4.2.1 SM - Definition of keys and values - Generation of IRK.
             * </p>
             */
            jau::uint128_t getIRKPtr() const noexcept { return pdu.get_uint128_nc(1); }

            std::string getName() const noexcept override {
                return "SMPSignInfo";
            }

        protected:
            std::string valueString() const noexcept override {
                return "size "+std::to_string(getDataSize())+", data anon";
            }
    };

    /**
     * Vol 3, Part H: 3.6.7 Security Request message.
     * <pre>
     * Vol 3 (Host), Part H (SM): 3 (SMP), 3.6 SECURITY IN BLUETOOTH LOW ENERGY
     * </pre>
     *
     * Opcode::SECURITY_REQUEST
     *
     * <pre>
     * [uint8_t opcode]
     * uint8_t auth_req_mask
     * </pre>
     *
     * Message is used by the slave to request that the master initiates security with the requested security properties,<br>
     * see Vol 3 (Host), Part H (SM): 2 (SM), 2.4 SECURITY IN BLUETOOTH LOW ENERGY, 2.4.6 Slave Security Request
     */
    class SMPSecurityReqMsg : public SMPPDUMsg
    {
        private:
            SMPAuthReqs authReqMask;
        public:
            SMPSecurityReqMsg(const uint8_t* source, const jau::nsize_t length)
            : SMPPDUMsg(source, length),
              authReqMask(static_cast<SMPAuthReqs>( pdu.get_uint8_nc(1) ))
            {
                checkOpcode(Opcode::SECURITY_REQUEST);
            }

            SMPSecurityReqMsg(const SMPAuthReqs auth_req_mask)
            : SMPPDUMsg(Opcode::SECURITY_REQUEST, 1+1), authReqMask(auth_req_mask)
            {
                pdu.put_uint8_nc(1, direct_bt::number(authReqMask));
            }

            jau::nsize_t getDataSize() const noexcept override {
                return 1;
            }

            /**
             * Returns the SMPPairingMsg::AuthRequirements (1 octet)
             * <p>
             * The AuthReq field is a bit field that indicates the requested security properties,<br>
             * see Vol 3 (Host), Part H (SM): 2 (SM), 2.3 Pairing Methods, 2.3.1 Security Properties,<br>
             * for the STK or LTK and GAP bonding information,<br>
             * see Vol 3 (Host), Part C (GAP): 9.4 Bonding Modes and Procedures<br>
             * </p>
             */
            constexpr SMPAuthReqs getAuthReqMask() const noexcept {
                return authReqMask;
            }
            constexpr bool isAuthRequirementBitSet(const SMPAuthReqs bit) const noexcept {
                return isSMPAuthReqBitSet(authReqMask, bit);
            }

            std::string getName() const noexcept override {
                return "SMPSecurityReq";
            }

        protected:
            std::string valueString() const noexcept override {
                return "auth_req "+getSMPAuthReqMaskString(getAuthReqMask());
            }
    };

} // namespace direct_bt

#endif /* SMP_TYPES_HPP_ */