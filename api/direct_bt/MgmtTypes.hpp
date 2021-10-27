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

#ifndef MGMT_TYPES_HPP_
#define MGMT_TYPES_HPP_

#include <cstring>
#include <string>
#include <cstdint>

#include <mutex>

#include <jau/function_def.hpp>
#include <jau/cow_darray.hpp>
#include <jau/octets.hpp>
#include <jau/packed_attribute.hpp>

#include "BTTypes0.hpp"
#include "BTIoctl.hpp"
#include "HCIComm.hpp"

#include "BTTypes1.hpp"
#include "SMPTypes.hpp"

namespace direct_bt {

    class MgmtException : public jau::RuntimeException {
        protected:
            MgmtException(std::string const type, std::string const m, const char* file, int line) noexcept
            : RuntimeException(type, m, file, line) {}

        public:
            MgmtException(std::string const m, const char* file, int line) noexcept
            : RuntimeException("MgmtException", m, file, line) {}
    };

    class MgmtOpcodeException : public MgmtException {
        public:
            MgmtOpcodeException(std::string const m, const char* file, int line) noexcept
            : MgmtException("MgmtOpcodeException", m, file, line) {}
    };

    enum MgmtConstU16 : uint16_t {
        MGMT_INDEX_NONE          = 0xFFFF,
        /* Net length, guaranteed to be null-terminated */
        MGMT_MAX_NAME_LENGTH        = 248+1,
        MGMT_MAX_SHORT_NAME_LENGTH  =  10+1
    };


    enum MgmtSizeConst : jau::nsize_t {
        MGMT_HEADER_SIZE       = 6
    };

    enum class MgmtStatus : uint8_t {
        SUCCESS             = 0x00,
        UNKNOWN_COMMAND     = 0x01,
        NOT_CONNECTED       = 0x02,
        FAILED              = 0x03,
        CONNECT_FAILED      = 0x04,
        AUTH_FAILED         = 0x05,
        NOT_PAIRED          = 0x06,
        NO_RESOURCES        = 0x07,
        TIMEOUT             = 0x08,
        ALREADY_CONNECTED   = 0x09,
        BUSY                = 0x0a,
        REJECTED            = 0x0b,
        NOT_SUPPORTED       = 0x0c,
        INVALID_PARAMS      = 0x0d,
        DISCONNECTED        = 0x0e,
        NOT_POWERED         = 0x0f,
        CANCELLED           = 0x10,
        INVALID_INDEX       = 0x11,
        RFKILLED            = 0x12,
        ALREADY_PAIRED      = 0x13,
        PERMISSION_DENIED   = 0x14
    };
    std::string to_string(const MgmtStatus opc) noexcept;
    HCIStatusCode to_HCIStatusCode(const MgmtStatus mstatus) noexcept;

    enum MgmtOption : uint32_t {
        EXTERNAL_CONFIG     = 0x00000001,
        PUBLIC_ADDRESS      = 0x00000002
    };

    /**
     * Link Key Types compatible with Mgmt's MgmtLinkKeyInfo
     */
    enum class MgmtLinkKeyType : uint8_t {
        /** Combination key */
        COMBI             = 0x00,
        /** Local Unit key */
        LOCAL_UNIT        = 0x01,
        /** Remote Unit key */
        REMOTE_UNIT       = 0x02,
        /** Debug Combination key */
        DBG_COMBI         = 0x03,
        /** Unauthenticated Combination key from P-192 */
        UNAUTH_COMBI_P192 = 0x04,
        /** Authenticated Combination key from P-192 */
        AUTH_COMBI_P192   = 0x05,
        /** Changed Combination key */
        CHANGED_COMBI     = 0x06,
        /** Unauthenticated Combination key from P-256 */
        UNAUTH_COMBI_P256 = 0x07,
        /** Authenticated Combination key from P-256 */
        AUTH_COMBI_P256   = 0x08,
        /** Denoting no or invalid link key type */
        NONE              = 0xff
    };
    std::string to_string(const MgmtLinkKeyType type) noexcept;

    /**
     * Long Term Key Types compatible with Mgmt's MgmtLongTermKeyInfo
     */
    enum class MgmtLTKType : uint8_t {
        /** Unauthenticated long term key, implying legacy. */
        UNAUTHENTICATED      = 0x00, // master ? SMP_LTK : SMP_LTK_SLAVE
        /** Authenticated long term key, implying legacy and BTSecurityLevel::ENC_AUTH. */
        AUTHENTICATED        = 0x01, // master ? SMP_LTK : SMP_LTK_SLAVE
        /** Unauthenticated long term key from P-256, implying Secure Connection (SC). */
        UNAUTHENTICATED_P256 = 0x02, // SMP_LTK_P256
        /** Authenticated long term key from P-256, implying Secure Connection (SC) and BTSecurityLevel::ENC_AUTH_FIPS. */
        AUTHENTICATED_P256   = 0x03, // SMP_LTK_P256
        /** Debug long term key from P-256 */
        DEBUG_P256           = 0x04, // SMP_LTK_P256_DEBUG
        /** Denoting no or invalid long term key type */
        NONE                 = 0xff
    };
    std::string to_string(const MgmtLTKType type) noexcept;
    MgmtLTKType to_MgmtLTKType(const SMPLongTermKey::Property ltk_prop_mask) noexcept;

    /**
     * Signature Resolving Key Types compatible with Mgmt's MgmtSignatureResolvingKeyInfo
     */
    enum class MgmtCSRKType : uint8_t {
        /** Unauthenticated local key */
        UNAUTHENTICATED_LOCAL  = 0x00,
        /** Unauthenticated remote key */
        UNAUTHENTICATED_REMOTE = 0x01,
        /** Authenticated local key, implying ::BTSecurityLevel::ENC_AUTH or ::BTSecurityLevel::ENC_AUTH_FIPS. */
        AUTHENTICATED_LOCAL    = 0x02,
        /** Authenticated remote key, implying ::BTSecurityLevel::ENC_AUTH or ::BTSecurityLevel::ENC_AUTH_FIPS. */
        AUTHENTICATED_REMOTE   = 0x03,
        /** Denoting no or invalid signature resolving key type */
        NONE                   = 0xff
    };
    std::string to_string(const MgmtCSRKType type) noexcept;

    /**
     * Used for MgmtLoadLongTermKeyCmd and MgmtEvtNewLongTermKey
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as a byte stream.
     * </p>
     */
    __pack( struct MgmtLongTermKeyInfo {
        EUI48 address;
        /** 0 reserved, 1 le public, 2 le static random address. Compatible with BDAddressType. */
        BDAddressType address_type;
        /** Describing type of key, i.e. used ::BTSecurityLevel and whether using Secure Connections (SC) for P256. */
        MgmtLTKType key_type;
        /**
         * BlueZ claims itself (initiator) as the SLAVE and the responder as the MASTER,
         * contrary to the spec roles of: Initiator (LL Master) and Responder (LL Slave).
         */
        uint8_t master;
        /** Encryption Size */
        uint8_t enc_size;
        /** Encryption Diversifier */
        uint16_t ediv;
        /** Random Number */
        uint64_t rand;
        /** Long Term Key (LTK) */
        jau::uint128_t ltk;

        std::string toString() const noexcept { // hex-fmt aligned with btmon
            return "LTK[address["+address.toString()+", "+to_string(address_type)+
                   "], type "+to_string(key_type)+", master "+jau::to_hexstring(master)+
                   ", enc_size "+std::to_string(enc_size)+
                   ", ediv "+jau::bytesHexString(reinterpret_cast<const uint8_t *>(&ediv), 0, sizeof(ediv), false /* lsbFirst */)+
                   ", rand "+jau::bytesHexString(reinterpret_cast<const uint8_t *>(&rand), 0, sizeof(rand), false /* lsbFirst */)+
                   ", ltk "+jau::bytesHexString(ltk.data, 0, sizeof(ltk), true /* lsbFirst */)+
                   "]";
        }

        /**
         * Convert this instance into its platform agnostic SMPLongTermKeyInfo type.
         */
        SMPLongTermKey toSMPLongTermKeyInfo() const noexcept {
            direct_bt::SMPLongTermKey res;
            res.clear();
            if( master ) {
                res.properties |= SMPLongTermKey::Property::RESPONDER;
            }
            switch( key_type ) {
                case MgmtLTKType::NONE:
                    res.clear();
                    break;
                case MgmtLTKType::UNAUTHENTICATED: //      = 0x00, // master ? SMP_LTK : SMP_LTK_SLAVE
                    break;
                case MgmtLTKType::AUTHENTICATED: //        = 0x01, // master ? SMP_LTK : SMP_LTK_SLAVE
                    res.properties |= SMPLongTermKey::Property::AUTH;
                    break;
                case MgmtLTKType::UNAUTHENTICATED_P256: // = 0x02, // SMP_LTK_P256
                    res.properties |= SMPLongTermKey::Property::SC;
                    break;
                case MgmtLTKType::AUTHENTICATED_P256: //   = 0x03, // SMP_LTK_P256
                    res.properties |= SMPLongTermKey::Property::SC;
                    res.properties |= SMPLongTermKey::Property::AUTH;
                    break;
                case MgmtLTKType::DEBUG_P256: //           = 0x04, // SMP_LTK_P256_DEBUG
                    res.properties |= SMPLongTermKey::Property::SC;
                    break;
            }
            res.enc_size = enc_size;
            res.ediv = ediv;
            res.rand = rand;
            res.ltk = ltk;
            return res;
        }
    } );

    /**
     * Used for MgmtLoadIdentityResolvingKeyCmd and MgmtEvtNewIdentityResolvingKey
     */
    __pack( struct MgmtIdentityResolvingKeyInfo {
        EUI48 address;
        BDAddressType address_type;
        jau::uint128_t irk;

        std::string toString() const noexcept {
            return "IRK[address["+address.toString()+", "+to_string(address_type)+
                   "], irk "+jau::bytesHexString(irk.data, 0, sizeof(irk), true /* lsbFirst */)+
                   "]";
        }
    } );

    /**
     * Used for MgmtEvtNewSignatureResolvingKey
     * <p>
     * One way for ATT Signed Write.
     * </p>
     */
    __pack( struct MgmtSignatureResolvingKeyInfo {
        EUI48 address;
        BDAddressType address_type;
        MgmtCSRKType key_type;
        jau::uint128_t csrk;

        std::string toString() const noexcept {
            return "CSRK[address["+address.toString()+", "+to_string(address_type)+
                   "], type "+to_string(key_type)+
                   ", csrk "+jau::bytesHexString(csrk.data, 0, sizeof(csrk), true /* lsbFirst */)+
                   "]";
        }
    } );

    /**
     * Used for MgmtLoadLinkKeyCmd and MgmtEvtNewLinkKey
     * <p>
     * Notable: No endian wise conversion shall occur on this data,
     *          since the encryption values are interpreted as a byte stream.
     * </p>
     */
    __pack( struct MgmtLinkKeyInfo {
        EUI48 address;
        BDAddressType address_type;
        MgmtLinkKeyType key_type;
        jau::uint128_t key;
        uint8_t pin_length;

        std::string toString() const noexcept {
            return "LK[address["+address.toString()+", "+to_string(address_type)+
                   "], type "+to_string(key_type)+
                   ", key "+jau::bytesHexString(key.data, 0, sizeof(key), true /* lsbFirst */)+
                   ", pinLen "+jau::to_hexstring(pin_length)+
                   "]";
        }

        /**
         * Convert this instance into its platform agnostic SMPLinkKeyInfo type.
         */
        SMPLinkKey toSMPLinkKeyInfo(const bool isResponder) const noexcept {
            direct_bt::SMPLinkKey res;
            res.clear();
            res.responder = isResponder;
            res.type = static_cast<SMPLinkKey::KeyType>(key_type);
            res.key = key;
            res.pin_length = pin_length;
            return res;
        }
    } );

    class MgmtMsg
    {
        protected:
            jau::POctets pdu;
            uint64_t ts_creation;

            virtual std::string baseString() const noexcept {
                return "opcode "+jau::to_hexstring(getIntOpcode())+", devID "+jau::to_hexstring(getDevID());
            }

            virtual std::string valueString() const noexcept = 0;

        public:
            static uint16_t getIntOpcode(const uint8_t * buffer) {
                return jau::get_uint16(buffer, 0, true /* littleEndian */);
            }
            static uint16_t getDevID(const uint8_t *data) {
                return jau::get_uint16(data, 2, true /* littleEndian */);
            }

            MgmtMsg(const uint16_t opc, const uint16_t dev_id, const uint16_t param_size)
            : pdu(MGMT_HEADER_SIZE+param_size, jau::endian::little),
              ts_creation(jau::getCurrentMilliseconds())
            {
                pdu.put_uint16_nc(0, opc);
                pdu.put_uint16_nc(2, dev_id);
                pdu.put_uint16_nc(4, param_size);
            }

            MgmtMsg(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : pdu(buffer, buffer_len, jau::endian::little),
              ts_creation(jau::getCurrentMilliseconds())
            {}

            virtual ~MgmtMsg() {}

            /**
             * Clone template for convenience, based on derived class's copy-constructor.<br>
             * MgmtEvent callback example:
             * <pre>
             * bool mgmtEvDeviceUnpairedMgmt(const MgmtEvent& e) noexcept {
             *     const MgmtEvtDeviceUnpaired &event = *static_cast<const MgmtEvtDeviceUnpaired *>(&e);
             *     MgmtMsg * b1 = MgmtMsg::clone(event);
             *     MgmtEvtDeviceUnpaired * b2 = MgmtMsg::clone(event);
             *     .. do something ..
             * }
             * </pre>
             * @tparam T The derived definite class type, deducible by source argument
             * @param source the source to be copied
             * @return a new instance.
             */
            template<class T>
            static T* clone(const T& source) noexcept { return new T(source); }

            uint64_t getTimestamp() const noexcept { return ts_creation; }

            jau::nsize_t getTotalSize() const noexcept { return pdu.size(); }

            /** Return the underlying octets read only */
            jau::TROOctets & getPDU() noexcept { return pdu; }

            uint16_t getIntOpcode() const noexcept { return pdu.get_uint16_nc(0); }
            uint16_t getDevID() const noexcept { return pdu.get_uint16_nc(2); }
            uint16_t getParamSize() const noexcept { return pdu.get_uint16_nc(4); }

            virtual std::string toString() const noexcept = 0;
    };

    class MgmtCommand : public MgmtMsg
    {
        public:
            enum class Opcode : uint16_t {
                READ_VERSION            = 0x0001,
                READ_COMMANDS           = 0x0002,
                READ_INDEX_LIST         = 0x0003,
                READ_INFO               = 0x0004,
                SET_POWERED             = 0x0005, // uint8_t bool
                SET_DISCOVERABLE        = 0x0006, // uint8_t bool [+ uint16_t timeout]
                SET_CONNECTABLE         = 0x0007, // uint8_t bool
                SET_FAST_CONNECTABLE    = 0x0008, // uint8_t bool
                SET_BONDABLE            = 0x0009, // uint8_t bool
                SET_LINK_SECURITY       = 0x000A,
                /** Secure Simple Pairing: 0x00 disabled, 0x01 enable. SSP only available for BREDR >= 2.1 not single-mode LE. */
                SET_SSP                 = 0x000B,
                SET_HS                  = 0x000C,
                SET_LE                  = 0x000D, // uint8_t bool
                SET_DEV_CLASS           = 0x000E, // uint8_t major, uint8_t minor
                SET_LOCAL_NAME          = 0x000F, // uint8_t name[MAX_NAME_LENGTH], uint8_t short_name[MAX_SHORT_NAME_LENGTH];
                ADD_UUID                = 0x0010,
                REMOVE_UUID             = 0x0011,
                LOAD_LINK_KEYS          = 0x0012,
                LOAD_LONG_TERM_KEYS     = 0x0013,
                DISCONNECT              = 0x0014,
                GET_CONNECTIONS         = 0x0015,
                PIN_CODE_REPLY          = 0x0016,
                PIN_CODE_NEG_REPLY      = 0x0017,
                SET_IO_CAPABILITY       = 0x0018, // SMPPairingMsg::IOCapability value
                PAIR_DEVICE             = 0x0019,
                CANCEL_PAIR_DEVICE      = 0x001A,
                UNPAIR_DEVICE           = 0x001B,
                USER_CONFIRM_REPLY      = 0x001C,
                USER_CONFIRM_NEG_REPLY  = 0x001D,
                USER_PASSKEY_REPLY      = 0x001E,
                USER_PASSKEY_NEG_REPLY  = 0x001F,
                READ_LOCAL_OOB_DATA     = 0x0020,
                ADD_REMOTE_OOB_DATA     = 0x0021,
                REMOVE_REMOTE_OOB_DATA  = 0x0022,
                START_DISCOVERY         = 0x0023, // MgmtUint8Cmd
                STOP_DISCOVERY          = 0x0024, // MgmtUint8Cmd
                CONFIRM_NAME            = 0x0025,
                BLOCK_DEVICE            = 0x0026,
                UNBLOCK_DEVICE          = 0x0027,
                SET_DEVICE_ID           = 0x0028,
                SET_ADVERTISING         = 0x0029,
                SET_BREDR               = 0x002A,
                SET_STATIC_ADDRESS      = 0x002B,
                SET_SCAN_PARAMS         = 0x002C,
                /** LE Secure Connections: 0x01 enables SC mixed, 0x02 enables SC only mode; Core Spec >= 4.1 */
                SET_SECURE_CONN         = 0x002D, // uint8_t 0x00 disabled, 0x01 enables mixed, 0x02 enables only mode; Core Spec >= 4.1
                SET_DEBUG_KEYS          = 0x002E, // uint8_t 0x00 disabled, 0x01 transient, 0x02 transient w/ controller mode
                SET_PRIVACY             = 0x002F,
                LOAD_IRKS               = 0x0030,
                GET_CONN_INFO           = 0x0031,
                GET_CLOCK_INFO          = 0x0032,
                ADD_DEVICE_WHITELIST    = 0x0033,
                REMOVE_DEVICE_WHITELIST = 0x0034,
                LOAD_CONN_PARAM         = 0x0035,
                READ_UNCONF_INDEX_LIST  = 0x0036,
                READ_CONFIG_INFO        = 0x0037,
                SET_EXTERNAL_CONFIG     = 0x0038,
                SET_PUBLIC_ADDRESS      = 0x0039,
                START_SERVICE_DISCOVERY = 0x003A,
                READ_LOCAL_OOB_EXT_DATA = 0x003B,
                READ_EXT_INDEX_LIST     = 0x003C,
                READ_ADV_FEATURES       = 0x003D,
                ADD_ADVERTISING         = 0x003E,
                REMOVE_ADVERTISING      = 0x003F,
                GET_ADV_SIZE_INFO       = 0x0040,
                START_LIMITED_DISCOVERY = 0x0041,
                READ_EXT_INFO           = 0x0042,
                SET_APPEARANCE          = 0x0043,
                GET_PHY_CONFIGURATION   = 0x0044, // linux >= 4.19
                SET_PHY_CONFIGURATION   = 0x0045, // linux >= 4.19
                SET_BLOCKED_KEYS        = 0x0046, // linux >= 5.6
                SET_WIDEBAND_SPEECH     = 0x0047, // linux >= 5.7
                READ_SECURITY_INFO      = 0x0048, // linux >= 5.8
                READ_EXP_FEATURES_INFO  = 0x0049, // linux >= 5.8
                SET_EXP_FEATURE         = 0x004a, // linux >= 5.8
                READ_DEF_SYSTEM_CONFIG  = 0x004b, // linux >= 5.9
                SET_DEF_SYSTEM_CONFIG   = 0x004c,
                READ_DEF_RUNTIME_CONFIG = 0x004d,
                SET_DEF_RUNTIME_CONFIG  = 0x004e,
                GET_DEVICE_FLAGS        = 0x004f,
                SET_DEVICE_FLAGS        = 0x0050,
                READ_ADV_MONITOR_FEATURES = 0x0051,
                ADD_ADV_PATTERNS_MONITOR  = 0x0052,
                REMOVE_ADV_MONITOR      = 0x0053 // linux >= 5.9
            };
            static constexpr uint16_t number(const Opcode rhs) noexcept {
                return static_cast<uint16_t>(rhs);
            }
            static std::string getOpcodeString(const Opcode op) noexcept;

        protected:
            inline static void checkOpcode(const Opcode has, const Opcode min, const Opcode max)
            {
                if( has < min || has > max ) {
                    throw MgmtOpcodeException("Has opcode "+jau::to_hexstring(static_cast<uint16_t>(has))+
                                     ", not within range ["+jau::to_hexstring(static_cast<uint16_t>(min))+
                                     ".."+jau::to_hexstring(static_cast<uint16_t>(max))+"]", E_FILE_LINE);
                }
            }
            static void checkOpcode(const Opcode has, const Opcode exp)
            {
                if( has != exp ) {
                    throw MgmtOpcodeException("Has evcode "+jau::to_hexstring(static_cast<uint16_t>(has))+
                                     ", not matching "+jau::to_hexstring(static_cast<uint16_t>(exp)), E_FILE_LINE);
                }
            }

            virtual std::string baseString() const noexcept override {
                return "opcode "+getOpcodeString(getOpcode())+", devID "+jau::to_hexstring(getDevID());
            }

            virtual std::string valueString() const noexcept override {
                const jau::nsize_t psz = getParamSize();
                const std::string ps = psz > 0 ? jau::bytesHexString(getParam(), 0, psz, true /* lsbFirst */) : "";
                return "param[size "+std::to_string(getParamSize())+", data "+ps+"], tsz "+std::to_string(getTotalSize());
            }

        public:

            MgmtCommand(const Opcode opc, const uint16_t dev_id, const uint16_t param_size=0)
            : MgmtMsg(number(opc), dev_id, param_size)
            {
                checkOpcode(opc, Opcode::READ_VERSION, Opcode::SET_BLOCKED_KEYS);
            }

            MgmtCommand(const Opcode opc, const uint16_t dev_id, const uint16_t param_size, const uint8_t* param)
            : MgmtCommand(opc, dev_id, param_size)
            {
                if( param_size > 0 ) {
                    memcpy(pdu.get_wptr_nc(MGMT_HEADER_SIZE), param, param_size);
                }
            }
            virtual ~MgmtCommand() noexcept override {}

            Opcode getOpcode() const noexcept { return static_cast<Opcode>( pdu.get_uint16_nc(0) ); }

            const uint8_t* getParam() const noexcept { return pdu.get_ptr_nc(MGMT_HEADER_SIZE); }

            std::string toString() const noexcept override {
                return "MgmtCmd["+baseString()+", "+valueString()+"]";
            }
    };

    class MgmtUint8Cmd : public MgmtCommand
    {
        public:
            MgmtUint8Cmd(const Opcode opc, const uint16_t dev_id, const uint8_t data)
            : MgmtCommand(opc, dev_id, 1)
            {
                pdu.put_uint8_nc(MGMT_HEADER_SIZE, data);
            }
    };

    /**
     * uint8_t discoverable
     * uint16_t timeout
     */
    class MgmtSetDiscoverableCmd : public MgmtCommand
    {
        protected:
            std::string valueString() const noexcept override {
                const std::string ps = "state '"+jau::to_hexstring(getDiscoverable())+"', timeout "+std::to_string(getTimeout())+"s";
                return "param[size "+std::to_string(getParamSize())+", data["+ps+"]], tsz "+std::to_string(getTotalSize());
            }

        public:
            MgmtSetDiscoverableCmd(const uint16_t dev_id, const uint8_t discoverable, uint16_t timeout_sec)
            : MgmtCommand(Opcode::SET_DISCOVERABLE, dev_id, 1+2)
            {
                pdu.put_uint8_nc(MGMT_HEADER_SIZE, discoverable);
                pdu.put_uint16_nc(MGMT_HEADER_SIZE+1, timeout_sec);
            }
            uint8_t getDiscoverable() const noexcept { return pdu.get_uint8_nc(MGMT_HEADER_SIZE); }
            uint16_t getTimeout() const noexcept { return pdu.get_uint16_nc(MGMT_HEADER_SIZE+1); }
    };

    /**
     * uint8_t name[MGMT_MAX_NAME_LENGTH];
     * uint8_t short_name[MGMT_MAX_SHORT_NAME_LENGTH];
     */
    class MgmtSetLocalNameCmd : public MgmtCommand
    {
        protected:
            std::string valueString() const noexcept override {
                const std::string ps = "name '"+getName()+"', shortName '"+getShortName()+"'";
                return "param[size "+std::to_string(getParamSize())+", data["+ps+"]], tsz "+std::to_string(getTotalSize());
            }

        public:
            MgmtSetLocalNameCmd(const uint16_t dev_id, const std::string & name, const std::string & short_name)
            : MgmtCommand(Opcode::SET_LOCAL_NAME, dev_id, MgmtConstU16::MGMT_MAX_NAME_LENGTH + MgmtConstU16::MGMT_MAX_SHORT_NAME_LENGTH)
            {
                pdu.put_string_nc(MGMT_HEADER_SIZE, name, MgmtConstU16::MGMT_MAX_NAME_LENGTH, true);
                pdu.put_string_nc(MGMT_HEADER_SIZE+MgmtConstU16::MGMT_MAX_NAME_LENGTH, short_name, MgmtConstU16::MGMT_MAX_SHORT_NAME_LENGTH, true);
            }
            const std::string getName() const noexcept { return pdu.get_string_nc(MGMT_HEADER_SIZE); }
            const std::string getShortName() const noexcept { return pdu.get_string_nc(MGMT_HEADER_SIZE + MgmtConstU16::MGMT_MAX_NAME_LENGTH); }
    };

    /**
     * uint8_t debug_keys,
     * uint16_t key_count,
     * MgmtLinkKey keys[key_count]
     */
    class MgmtLoadLinkKeyCmd : public MgmtCommand
    {
        private:
            void checkParamIdx(const jau::nsize_t idx) const {
                const jau::nsize_t kc = getKeyCount();
                if( idx >= kc ) {
                    throw jau::IndexOutOfBoundsException(idx, kc, E_FILE_LINE);
                }
            }

        protected:
            std::string valueString() const noexcept override {
                const jau::nsize_t keyCount = getKeyCount();
                std::string ps = "count "+std::to_string(keyCount)+": ";
                for(jau::nsize_t i=0; i<keyCount; i++) {
                    if( 0 < i ) {
                        ps.append(", ");
                    }
                    ps.append( getLinkKey(i).toString() );
                }
                return "param[size "+std::to_string(getParamSize())+", data["+ps+"]], tsz "+std::to_string(getTotalSize());
            }

        public:
            MgmtLoadLinkKeyCmd(const uint16_t dev_id, const bool debug_keys, const MgmtLinkKeyInfo & key)
            : MgmtCommand(Opcode::LOAD_LINK_KEYS, dev_id, 1 + 2 + sizeof(MgmtLinkKeyInfo))
            {
                pdu.put_uint8_nc(MGMT_HEADER_SIZE, debug_keys ? 0x01 : 0x00);
                pdu.put_uint16_nc(MGMT_HEADER_SIZE+1, 1);
                memcpy(pdu.get_wptr_nc(MGMT_HEADER_SIZE+1+2), &key, sizeof(MgmtLinkKeyInfo));
            }

            MgmtLoadLinkKeyCmd(const uint16_t dev_id, const bool debug_keys, const jau::darray<MgmtLinkKeyInfo> &keys)
            : MgmtCommand(Opcode::LOAD_LINK_KEYS, dev_id, 1 + 2 + keys.size() * sizeof(MgmtLinkKeyInfo))
            {
                jau::nsize_t offset = MGMT_HEADER_SIZE;
                pdu.put_uint8_nc(offset, debug_keys ? 0x01 : 0x00); offset+= 1;
                pdu.put_uint16_nc(offset, keys.size()); offset+= 2;

                for(auto it = keys.begin(); it != keys.end(); ++it, offset+=sizeof(MgmtLinkKeyInfo)) {
                    const MgmtLinkKeyInfo& key = *it;
                    memcpy(pdu.get_wptr_nc(offset), &key, sizeof(MgmtLinkKeyInfo));
                }
            }

            bool getDebugKeys() const noexcept { return 0 != pdu.get_uint8_nc(MGMT_HEADER_SIZE); }

            uint16_t getKeyCount() const noexcept { return pdu.get_uint16_nc(MGMT_HEADER_SIZE+1); }

            const MgmtLinkKeyInfo& getLinkKey(jau::nsize_t idx) const {
                checkParamIdx(idx);
                return *reinterpret_cast<const MgmtLinkKeyInfo *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 2 + sizeof(MgmtLinkKeyInfo)*idx) );
            }
    };

    /**
     * uint16_t key_count
     * MgmtLongTermKey keys[key_count]
     */
    class MgmtLoadLongTermKeyCmd : public MgmtCommand
    {
        private:
            void checkParamIdx(const jau::nsize_t idx) const {
                const jau::nsize_t kc = getKeyCount();
                if( idx >= kc ) {
                    throw jau::IndexOutOfBoundsException(idx, kc, E_FILE_LINE);
                }
            }

        protected:
            std::string valueString() const noexcept override {
                const jau::nsize_t keyCount = getKeyCount();
                std::string ps = "count "+std::to_string(keyCount)+": ";
                for(jau::nsize_t i=0; i<keyCount; i++) {
                    if( 0 < i ) {
                        ps.append(", ");
                    }
                    ps.append( getLongTermKey(i).toString() );
                }
                return "param[size "+std::to_string(getParamSize())+", data["+ps+"]], tsz "+std::to_string(getTotalSize());
            }

        public:
            MgmtLoadLongTermKeyCmd(const uint16_t dev_id, const MgmtLongTermKeyInfo & key)
            : MgmtCommand(Opcode::LOAD_LONG_TERM_KEYS, dev_id, 2 + sizeof(MgmtLongTermKeyInfo))
            {
                pdu.put_uint16_nc(MGMT_HEADER_SIZE, 1);
                memcpy(pdu.get_wptr_nc(MGMT_HEADER_SIZE+2), &key, sizeof(MgmtLongTermKeyInfo));
            }

            MgmtLoadLongTermKeyCmd(const uint16_t dev_id, const jau::darray<MgmtLongTermKeyInfo> &keys)
            : MgmtCommand(Opcode::LOAD_LONG_TERM_KEYS, dev_id, 2 + keys.size() * sizeof(MgmtLongTermKeyInfo))
            {
                jau::nsize_t offset = MGMT_HEADER_SIZE;
                pdu.put_uint16_nc(offset, keys.size()); offset+= 2;

                for(auto it = keys.begin(); it != keys.end(); ++it, offset+=sizeof(MgmtLongTermKeyInfo)) {
                    const MgmtLongTermKeyInfo& key = *it;
                    memcpy(pdu.get_wptr_nc(offset), &key, sizeof(MgmtLongTermKeyInfo));
                }
            }
            uint16_t getKeyCount() const noexcept { return pdu.get_uint16_nc(MGMT_HEADER_SIZE); }

            const MgmtLongTermKeyInfo& getLongTermKey(jau::nsize_t idx) const {
                checkParamIdx(idx);
                return *reinterpret_cast<const MgmtLongTermKeyInfo *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 2 + sizeof(MgmtLongTermKeyInfo)*idx) );
            }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtCmdAdressInfoMeta : public MgmtCommand
    {
        protected:
            std::string valueString() const noexcept override {
                const std::string ps = "address "+getAddress().toString()+", addressType "+to_string(getAddressType());
                return "param[size "+std::to_string(getParamSize())+", data["+ps+"]], tsz "+std::to_string(getTotalSize());
            }

        public:
            MgmtCmdAdressInfoMeta(const Opcode opc, const uint16_t dev_id, const BDAddressAndType& addressAndType)
            : MgmtCommand(opc, dev_id, 6+1)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
            }

            virtual ~MgmtCmdAdressInfoMeta() noexcept override {}

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtDisconnectCmd : public MgmtCmdAdressInfoMeta
    {
        public:
            MgmtDisconnectCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType)
            : MgmtCmdAdressInfoMeta(Opcode::DISCONNECT, dev_id, addressAndType)
            { }
    };

    /**
     * uint16_t key_count
     * MgmtIdentityResolvingKey keys[key_count]
     */
    class MgmtLoadIdentityResolvingKeyCmd : public MgmtCommand
    {
        private:
            void checkParamIdx(const jau::nsize_t idx) const {
                const jau::nsize_t kc = getKeyCount();
                if( idx >= kc ) {
                    throw jau::IndexOutOfBoundsException(idx, kc, E_FILE_LINE);
                }
            }

        protected:
            std::string valueString() const noexcept override {
                const jau::nsize_t keyCount = getKeyCount();
                std::string ps = "count "+std::to_string(keyCount)+": ";
                for(jau::nsize_t i=0; i<keyCount; i++) {
                    if( 0 < i ) {
                        ps.append(", ");
                    }
                    ps.append( getIdentityResolvingKey(i).toString() );
                }
                return "param[size "+std::to_string(getParamSize())+", data["+ps+"]], tsz "+std::to_string(getTotalSize());
            }

        public:
            MgmtLoadIdentityResolvingKeyCmd(const uint16_t dev_id, const MgmtIdentityResolvingKeyInfo & key)
            : MgmtCommand(Opcode::LOAD_IRKS, dev_id, 2 + sizeof(MgmtIdentityResolvingKeyInfo))
            {
                pdu.put_uint16_nc(MGMT_HEADER_SIZE, 1);
                memcpy(pdu.get_wptr_nc(MGMT_HEADER_SIZE+2), &key, sizeof(MgmtIdentityResolvingKeyInfo));
            }

            MgmtLoadIdentityResolvingKeyCmd(const uint16_t dev_id, const jau::darray<MgmtIdentityResolvingKeyInfo> &keys)
            : MgmtCommand(Opcode::LOAD_IRKS, dev_id, 2 + keys.size() * sizeof(MgmtIdentityResolvingKeyInfo))
            {
                jau::nsize_t offset = MGMT_HEADER_SIZE;
                pdu.put_uint16_nc(offset, keys.size()); offset+= 2;

                for(auto it = keys.begin(); it != keys.end(); ++it, offset+=sizeof(MgmtIdentityResolvingKeyInfo)) {
                    const MgmtIdentityResolvingKeyInfo& key = *it;
                    memcpy(pdu.get_wptr_nc(offset), &key, sizeof(MgmtIdentityResolvingKeyInfo));
                }
            }
            uint16_t getKeyCount() const noexcept { return pdu.get_uint16_nc(MGMT_HEADER_SIZE); }

            const MgmtIdentityResolvingKeyInfo& getIdentityResolvingKey(jau::nsize_t idx) const {
                checkParamIdx(idx);
                return *reinterpret_cast<const MgmtIdentityResolvingKeyInfo *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 2 + sizeof(MgmtIdentityResolvingKeyInfo)*idx) );
            }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtGetConnectionInfoCmd : public MgmtCmdAdressInfoMeta
    {
        public:
            MgmtGetConnectionInfoCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType)
            : MgmtCmdAdressInfoMeta(Opcode::GET_CONN_INFO, dev_id, addressAndType)
            { }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * uint8_t pin_len,
     * uint8_t pin_code[16]
     */
    class MgmtPinCodeReplyCmd : public MgmtCommand
    {
        protected:
            std::string valueString() const noexcept override {
                const std::string ps = "address "+getAddress().toString()+", addressType "+to_string(getAddressType())+
                                       ", pin "+getPinCode().toString();
                return "param[size "+std::to_string(getParamSize())+", data["+ps+"]], tsz "+std::to_string(getTotalSize());
            }

        public:
            MgmtPinCodeReplyCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType,
                                const uint8_t pin_len, const jau::TROOctets &pin_code)
            : MgmtCommand(Opcode::PIN_CODE_REPLY, dev_id, 6+1+1+16)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+7, pin_len);
                pdu.put_octets_nc(MGMT_HEADER_SIZE+8, pin_code);
            }
            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info
            uint8_t getPinLength() const noexcept { return pdu.get_uint8_nc(MGMT_HEADER_SIZE+6+1); }
            jau::TROOctets getPinCode() const noexcept { return jau::POctets(pdu.get_ptr_nc(MGMT_HEADER_SIZE+6+1+1), getPinLength(), jau::endian::little); }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtPinCodeNegativeReplyCmd : public MgmtCmdAdressInfoMeta
    {
        public:
            MgmtPinCodeNegativeReplyCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType)
            : MgmtCmdAdressInfoMeta(Opcode::PIN_CODE_NEG_REPLY, dev_id, addressAndType)
            { }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * SMPIOCapability io_cap (1 octet)
     */
    class MgmtPairDeviceCmd : public MgmtCommand
    {
        protected:
            std::string valueString() const noexcept override {
                const std::string ps = "address "+getAddress().toString()+", addressType "+to_string(getAddressType())+
                                       ", io "+to_string(getIOCapability());
                return "param[size "+std::to_string(getParamSize())+", data["+ps+"]], tsz "+std::to_string(getTotalSize());
            }

        public:
            MgmtPairDeviceCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType, const SMPIOCapability iocap)
            : MgmtCommand(Opcode::PAIR_DEVICE, dev_id, 6+1+1)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6+1, direct_bt::number(iocap));
            }
            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info
            SMPIOCapability getIOCapability() const noexcept { return to_SMPIOCapability( pdu.get_uint8_nc(MGMT_HEADER_SIZE+6+1) ); }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtCancelPairDevice : public MgmtCmdAdressInfoMeta
    {
        public:
        MgmtCancelPairDevice(const uint16_t dev_id, const BDAddressAndType& addressAndType)
            : MgmtCmdAdressInfoMeta(Opcode::CANCEL_PAIR_DEVICE, dev_id, addressAndType)
            { }
    };


    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * bool disconnect (1 octet)
     */
    class MgmtUnpairDeviceCmd : public MgmtCommand
    {
        protected:
            std::string valueString() const noexcept override {
                const std::string ps = "address "+getAddress().toString()+", addressType "+to_string(getAddressType())+
                                       ", disconnect "+std::to_string(getDisconnect());
                return "param[size "+std::to_string(getParamSize())+", data["+ps+"]], tsz "+std::to_string(getTotalSize());
            }

        public:
            MgmtUnpairDeviceCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType, const bool disconnect)
            : MgmtCommand(Opcode::UNPAIR_DEVICE, dev_id, 6+1+1)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6+1, disconnect ? 0x01 : 0x00);
            }
            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info
            bool getDisconnect() const noexcept { return 0x00 != pdu.get_uint8_nc(MGMT_HEADER_SIZE+6+1); }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtUserConfirmReplyCmd : public MgmtCmdAdressInfoMeta
    {
        public:
            MgmtUserConfirmReplyCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType)
            : MgmtCmdAdressInfoMeta(Opcode::USER_CONFIRM_REPLY, dev_id, addressAndType)
            { }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtUserConfirmNegativeReplyCmd : public MgmtCmdAdressInfoMeta
    {
        public:
            MgmtUserConfirmNegativeReplyCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType)
            : MgmtCmdAdressInfoMeta(Opcode::USER_CONFIRM_NEG_REPLY, dev_id, addressAndType)
            { }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * uint32_t passkey
     */
    class MgmtUserPasskeyReplyCmd : public MgmtCommand
    {
        protected:
            std::string valueString() const noexcept override {
                const std::string ps = "address "+getAddress().toString()+", addressType "+to_string(getAddressType())+
                                       ", passkey "+jau::to_hexstring(getPasskey());
                return "param[size "+std::to_string(getParamSize())+", data["+ps+"]], tsz "+std::to_string(getTotalSize());
            }

        public:
            MgmtUserPasskeyReplyCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType, const uint32_t passkey)
            : MgmtCommand(Opcode::USER_PASSKEY_REPLY, dev_id, 6+1+4)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint32_nc(MGMT_HEADER_SIZE+6+1, passkey);
            }
            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info
            uint32_t getPasskey() const noexcept { return pdu.get_uint32_nc(MGMT_HEADER_SIZE+6+1); }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtUserPasskeyNegativeReplyCmd : public MgmtCmdAdressInfoMeta
    {
        public:
            MgmtUserPasskeyNegativeReplyCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType)
            : MgmtCmdAdressInfoMeta(Opcode::USER_PASSKEY_NEG_REPLY, dev_id, addressAndType)
            { }
    };

    // TODO READ_LOCAL_OOB_DATA     = 0x0020,
    // TODO ADD_REMOTE_OOB_DATA     = 0x0021,
    // TODO REMOVE_REMOTE_OOB_DATA  = 0x0022,

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * uint8_t action
     */
    class MgmtAddDeviceToWhitelistCmd : public MgmtCommand
    {
        protected:
            std::string valueString() const noexcept override {
                const std::string ps = "address "+getAddress().toString()+", addressType "+to_string(getAddressType())+
                                       ", connectionType "+std::to_string(static_cast<uint8_t>(getConnectionType()));
                return "param[size "+std::to_string(getParamSize())+", data["+ps+"]], tsz "+std::to_string(getTotalSize());
            }

        public:
            MgmtAddDeviceToWhitelistCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType, const HCIWhitelistConnectType ctype)
            : MgmtCommand(Opcode::ADD_DEVICE_WHITELIST, dev_id, 6+1+1)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6+1, direct_bt::number(ctype));
            }
            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info
            HCIWhitelistConnectType getConnectionType() const noexcept { return static_cast<HCIWhitelistConnectType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6+1)); }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtRemoveDeviceFromWhitelistCmd : public MgmtCmdAdressInfoMeta
    {
        public:
            MgmtRemoveDeviceFromWhitelistCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType)
            : MgmtCmdAdressInfoMeta(Opcode::REMOVE_DEVICE_WHITELIST, dev_id, addressAndType)
            { }
    };

    /**
     * Used in MgmtLoadConnParamCmd and MgmtEvtNewConnectionParam
     */
    __pack( struct MgmtConnParam {
        EUI48 address;
        BDAddressType address_type;
        uint16_t min_interval;
        uint16_t max_interval;
        uint16_t latency;
        uint16_t supervision_timeout;

        std::string toString() const noexcept {
            return "ConnParam[address "+address.toString()+", addressType "+to_string(address_type)+
                        ", interval["+std::to_string(min_interval)+".."+std::to_string(max_interval)+
                        "], latency "+std::to_string(latency)+", timeout "+std::to_string(supervision_timeout)+"]";
        }
    } );

    /**
     * uint16_t param_count                       2
     * MgmtConnParam param[]                     15 = 1x
     *
     * MgmtConnParam {
     *   mgmt_addr_info { EUI48, uint8_t type },  7
     *   uint16_t min_interval;                   2
     *   uint16_t max_interval;                   2
     *   uint16_t latency;                        2
     *   uint16_t timeout;                        2
     * }
     * uint8_t name[MGMT_MAX_NAME_LENGTH];
     * uint8_t short_name[MGMT_MAX_SHORT_NAME_LENGTH];
     */
    class MgmtLoadConnParamCmd : public MgmtCommand
    {
        private:
            void checkParamIdx(const jau::nsize_t idx) const {
                const jau::nsize_t pc = getParamCount();
                if( idx >= pc ) {
                    throw jau::IndexOutOfBoundsException(idx, pc, E_FILE_LINE);
                }
            }

        protected:
            std::string valueString() const noexcept override {
                const jau::nsize_t paramCount = getParamCount();
                std::string ps = "count "+std::to_string(paramCount)+": ";
                for(jau::nsize_t i=0; i<paramCount; i++) {
                    if( 0 < i ) {
                        ps.append(", ");
                    }
                    ps.append( getConnParam(i).toString() );
                }
                return "param[size "+std::to_string(getParamSize())+", data["+ps+"]], tsz "+std::to_string(getTotalSize());
            }

        public:
            MgmtLoadConnParamCmd(const uint16_t dev_id, const MgmtConnParam & connParam)
            : MgmtCommand(Opcode::LOAD_CONN_PARAM, dev_id, 2 + sizeof(MgmtConnParam))
            {
                pdu.put_uint16_nc(MGMT_HEADER_SIZE, 1);
                memcpy(pdu.get_wptr_nc(MGMT_HEADER_SIZE+2), &connParam, sizeof(MgmtConnParam));
            }

            MgmtLoadConnParamCmd(const uint16_t dev_id, const jau::darray<MgmtConnParam> &connParams)
            : MgmtCommand(Opcode::LOAD_CONN_PARAM, dev_id, 2 + connParams.size() * sizeof(MgmtConnParam))
            {
                jau::nsize_t offset = MGMT_HEADER_SIZE;
                pdu.put_uint16_nc(offset, connParams.size()); offset+= 2;

                for(auto it = connParams.begin(); it != connParams.end(); ++it, offset+=sizeof(MgmtConnParam)) {
                    const MgmtConnParam& connParam = *it;
                    memcpy(pdu.get_wptr_nc(offset), &connParam, sizeof(MgmtConnParam));
                }
            }

            uint16_t getParamCount() const noexcept { return pdu.get_uint16_nc(MGMT_HEADER_SIZE); }

            const MgmtConnParam& getConnParam(jau::nsize_t idx) const {
                checkParamIdx(idx);
                return *reinterpret_cast<const MgmtConnParam *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 2 + sizeof(MgmtConnParam)*idx) );
            }
    };

    /**
     * uint16_t opcode,
     * uint16_t dev-id,
     * uint16_t param_size
     */
    class MgmtEvent : public MgmtMsg
    {
        public:
            enum class Opcode : uint16_t {
                INVALID                      = 0x0000,
                CMD_COMPLETE                 = 0x0001,
                CMD_STATUS                   = 0x0002,
                CONTROLLER_ERROR             = 0x0003,
                INDEX_ADDED                  = 0x0004,
                INDEX_REMOVED                = 0x0005,
                NEW_SETTINGS                 = 0x0006,
                CLASS_OF_DEV_CHANGED         = 0x0007,
                LOCAL_NAME_CHANGED           = 0x0008,
                NEW_LINK_KEY                 = 0x0009,
                NEW_LONG_TERM_KEY            = 0x000A,
                DEVICE_CONNECTED             = 0x000B,
                DEVICE_DISCONNECTED          = 0x000C,
                CONNECT_FAILED               = 0x000D,
                PIN_CODE_REQUEST             = 0x000E,
                USER_CONFIRM_REQUEST         = 0x000F,
                USER_PASSKEY_REQUEST         = 0x0010,
                AUTH_FAILED                  = 0x0011,
                DEVICE_FOUND                 = 0x0012,
                DISCOVERING                  = 0x0013,
                DEVICE_BLOCKED               = 0x0014,
                DEVICE_UNBLOCKED             = 0x0015,
                DEVICE_UNPAIRED              = 0x0016,
                PASSKEY_NOTIFY               = 0x0017,
                NEW_IRK                      = 0x0018,
                NEW_CSRK                     = 0x0019,
                DEVICE_WHITELIST_ADDED       = 0x001A,
                DEVICE_WHITELIST_REMOVED     = 0x001B,
                NEW_CONN_PARAM               = 0x001C,
                UNCONF_INDEX_ADDED           = 0x001D,
                UNCONF_INDEX_REMOVED         = 0x001E,
                NEW_CONFIG_OPTIONS           = 0x001F,
                EXT_INDEX_ADDED              = 0x0020,
                EXT_INDEX_REMOVED            = 0x0021,
                LOCAL_OOB_DATA_UPDATED       = 0x0022,
                ADVERTISING_ADDED            = 0x0023,
                ADVERTISING_REMOVED          = 0x0024,
                EXT_INFO_CHANGED             = 0x0025,
                PHY_CONFIGURATION_CHANGED    = 0x0026, // linux >= 4.19
                EXP_FEATURE_CHANGED          = 0x0027, // linux >= 5.8
                DEVICE_FLAGS_CHANGED         = 0x002a,
                ADV_MONITOR_ADDED            = 0x002b,
                ADV_MONITOR_REMOVED          = 0x002c,
                PAIR_DEVICE_COMPLETE         = 0x002d, // CMD_COMPLETE of PAIR_DEVICE (pending)
                HCI_ENC_CHANGED              = 0x002e, // direct_bt extension HCIHandler -> listener
                HCI_ENC_KEY_REFRESH_COMPLETE = 0x002f, // direct_bt extension HCIHandler -> listener
                HCI_LE_REMOTE_FEATURES       = 0x0030, // direct_bt extension HCIHandler -> listener
                HCI_LE_PHY_UPDATE_COMPLETE   = 0x0031, // direct_bt extension HCIHandler -> listener
                HCI_LE_LTK_REQUEST           = 0x0032, // direct_bt extension HCIHandler -> listener
                HCI_LE_LTK_REPLY_ACK         = 0x0033,
                HCI_LE_LTK_REPLY_REJ         = 0x0034,
                HCI_LE_ENABLE_ENC            = 0x0035,
                MGMT_EVENT_TYPE_COUNT        = 0x0036
            };
            static constexpr uint16_t number(const Opcode rhs) noexcept {
                return static_cast<uint16_t>(rhs);
            }
            static std::string getOpcodeString(const Opcode opc) noexcept;

        protected:
            inline static void checkOpcode(const Opcode has, const Opcode min, const Opcode max)
            {
                if( has < min || has > max ) {
                    throw MgmtOpcodeException("Has opcode "+jau::to_hexstring(static_cast<uint16_t>(has))+
                                     ", not within range ["+jau::to_hexstring(static_cast<uint16_t>(min))+
                                     ".."+jau::to_hexstring(static_cast<uint16_t>(max))+"]", E_FILE_LINE);
                }
            }
            static void checkOpcode(const Opcode has, const Opcode exp)
            {
                if( has != exp ) {
                    throw MgmtOpcodeException("Has opcode "+jau::to_hexstring(static_cast<uint16_t>(has))+
                                     ", not matching "+jau::to_hexstring(static_cast<uint16_t>(exp)), E_FILE_LINE);
                }
            }

            virtual std::string baseString() const noexcept override {
                return "opcode "+getOpcodeString(getOpcode())+", devID "+jau::to_hexstring(getDevID());
            }
            virtual std::string valueString() const noexcept override {
                const jau::nsize_t d_sz = getDataSize();
                const std::string d_str = d_sz > 0 ? jau::bytesHexString(getData(), 0, d_sz, true /* lsbFirst */) : "";
                return "data[size "+std::to_string(d_sz)+", data "+d_str+"], tsz "+std::to_string(getTotalSize());
            }

        public:
            static MgmtEvent::Opcode getOpcode(const uint8_t * buffer) {
                return static_cast<MgmtEvent::Opcode>( jau::get_uint16(buffer, 0, true /* littleEndian */) );
            }

            /**
             * Return a newly created specialized instance pointer to base class.
             * <p>
             * Returned memory reference is managed by caller (delete etc)
             * </p>
             */
            static std::unique_ptr<MgmtEvent> getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept;

            /** Persistent memory, w/ ownership ..*/
            MgmtEvent(const uint8_t* buffer, const jau::nsize_t buffer_len, const jau::nsize_t exp_param_size)
            : MgmtMsg(buffer, buffer_len)
            {
                const jau::nsize_t paramSize = getParamSize();
                pdu.check_range(0, MGMT_HEADER_SIZE+paramSize);
                if( exp_param_size > paramSize ) {
                    throw jau::IndexOutOfBoundsException(exp_param_size, paramSize, E_FILE_LINE);
                }
                checkOpcode(getOpcode(), Opcode::CMD_COMPLETE, Opcode::PHY_CONFIGURATION_CHANGED);
            }

            MgmtEvent(const Opcode opc, const uint16_t dev_id, const uint16_t param_size=0)
            : MgmtMsg(number(opc), dev_id, param_size)
            {
                // checkOpcode(opc, READ_VERSION, SET_BLOCKED_KEYS);
            }

            MgmtEvent(const Opcode opc, const uint16_t dev_id, const uint16_t param_size, const uint8_t* param)
            : MgmtEvent(opc, dev_id, param_size)
            {
                if( param_size > 0 ) {
                    memcpy(pdu.get_wptr_nc(MGMT_HEADER_SIZE), param, param_size);
                }
            }

            virtual ~MgmtEvent() noexcept override {}

            jau::nsize_t getTotalSize() const noexcept { return pdu.size(); }

            Opcode getOpcode() const noexcept { return static_cast<Opcode>( pdu.get_uint16_nc(0) ); }

            virtual jau::nsize_t getDataOffset() const noexcept { return MGMT_HEADER_SIZE; }
            virtual jau::nsize_t getDataSize() const noexcept { return getParamSize(); }
            virtual const uint8_t* getData() const noexcept { return getDataSize()>0 ? pdu.get_ptr_nc(getDataOffset()) : nullptr; }

            virtual bool validate(const MgmtCommand &req) const noexcept {
                return req.getDevID() == getDevID();
            }

            std::string toString() const noexcept override {
                return "MgmtEvt["+baseString()+", "+valueString()+"]";
            }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtEvtAdressInfoMeta : public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address="+getAddress().toString()+
                       ", addressType "+to_string(getAddressType());
            }

        public:
            MgmtEvtAdressInfoMeta(const Opcode opc, const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 7)
            {
                checkOpcode(getOpcode(), opc);
            }

            virtual ~MgmtEvtAdressInfoMeta() noexcept override {}

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+7; }
            jau::nsize_t getDataSize() const noexcept override { return getParamSize()-7; }
            const uint8_t* getData() const noexcept override { return getDataSize()>0 ? pdu.get_ptr_nc(getDataOffset()) : nullptr; }
    };

    class MgmtEvtCmdComplete : public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", cmd "+MgmtCommand::getOpcodeString(getCmdOpcode())+
                       ", status "+jau::to_hexstring(static_cast<uint8_t>(getStatus()))+" "+to_string(getStatus());
            }

            MgmtEvtCmdComplete(const uint8_t* buffer, const jau::nsize_t buffer_len, const jau::nsize_t exp_param_size)
            : MgmtEvent(buffer, buffer_len, 3+exp_param_size)
            {
                checkOpcode(getOpcode(), Opcode::CMD_COMPLETE);
            }

        public:

            static MgmtCommand::Opcode getCmdOpcode(const uint8_t *data) {
                return static_cast<MgmtCommand::Opcode>( jau::get_uint16(data, MGMT_HEADER_SIZE, true /* littleEndian */) );
            }
            static MgmtStatus getStatus(const uint8_t *data) {
                return static_cast<MgmtStatus>( jau::get_uint8(data, MGMT_HEADER_SIZE+2) );
            }

            MgmtEvtCmdComplete(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 3)
            {
                checkOpcode(getOpcode(), Opcode::CMD_COMPLETE);
            }

            virtual ~MgmtEvtCmdComplete() noexcept override {}

            MgmtCommand::Opcode getCmdOpcode() const noexcept { return static_cast<MgmtCommand::Opcode>( pdu.get_uint16_nc(MGMT_HEADER_SIZE) ); }
            MgmtStatus getStatus() const noexcept { return static_cast<MgmtStatus>( pdu.get_uint8_nc(MGMT_HEADER_SIZE+2) ); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+3; }
            jau::nsize_t getDataSize() const noexcept override { return getParamSize()-3; }
            const uint8_t* getData() const noexcept override { return getDataSize()>0 ? pdu.get_ptr_nc(getDataOffset()) : nullptr; }

            bool validate(const MgmtCommand &req) const noexcept override {
                return MgmtEvent::validate(req) && req.getOpcode() == getCmdOpcode();
            }

            /**
             * Returns AdapterSetting if getCmdOpcode() expects a single 4-octet AdapterSetting and hence getDataSize() == 4.
             */
            bool getCurrentSettings(AdapterSetting& current_settings) const noexcept;

            /**
             * Convert this instance into ConnectionInfo
             * if getCmdOpcode() == GET_CONN_INFO, getStatus() == SUCCESS and size allows,
             * otherwise returns nullptr.
             */
            std::shared_ptr<ConnectionInfo> toConnectionInfo() const noexcept;

            /**
             * Convert this instance into ConnectionInfo
             * if getCmdOpcode() == SET_LOCAL_NAME, getStatus() == SUCCESS and size allows,
             * otherwise returns nullptr.
             */
            std::shared_ptr<NameAndShortName> toNameAndShortName() const noexcept;
    };

    class MgmtEvtCmdStatus : public MgmtEvent
    {
        public:

        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", cmd "+MgmtCommand::getOpcodeString(getCmdOpcode())+
                       ", status "+jau::to_hexstring(static_cast<uint8_t>(getStatus()))+" "+to_string(getStatus());
            }

        public:
            MgmtEvtCmdStatus(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 3)
            {
                checkOpcode(getOpcode(), Opcode::CMD_STATUS);
            }
            MgmtCommand::Opcode getCmdOpcode() const noexcept { return static_cast<MgmtCommand::Opcode>( pdu.get_uint16_nc(MGMT_HEADER_SIZE) ); }
            MgmtStatus getStatus() const noexcept { return static_cast<MgmtStatus>( pdu.get_uint8_nc(MGMT_HEADER_SIZE+2) ); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+3; }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }

            bool validate(const MgmtCommand &req) const noexcept override {
                return MgmtEvent::validate(req) && req.getOpcode() == getCmdOpcode();
            }
    };

    class MgmtEvtControllerError : public MgmtEvent
    {
        public:

        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", error-code "+jau::to_hexstring(static_cast<uint8_t>(getErrorCode()));
            }

        public:
            MgmtEvtControllerError(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 1)
            {
                checkOpcode(getOpcode(), Opcode::CONTROLLER_ERROR);
            }
            uint8_t getErrorCode() const noexcept { return pdu.get_uint8_nc(MGMT_HEADER_SIZE); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+1; }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };

    /**
     * uint32_t settings
     */
    class MgmtEvtNewSettings : public MgmtEvent
    {
        public:

        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", settings="+to_string(getSettings());
            }

        public:
            MgmtEvtNewSettings(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 4)
            {
                checkOpcode(getOpcode(), Opcode::NEW_SETTINGS);
            }
            AdapterSetting getSettings() const noexcept { return static_cast<AdapterSetting>( pdu.get_uint32_nc(MGMT_HEADER_SIZE) ); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+4; }
            jau::nsize_t getDataSize() const noexcept override { return getParamSize()-4; }
            const uint8_t* getData() const noexcept override { return getDataSize()>0 ? pdu.get_ptr_nc(getDataOffset()) : nullptr; }
    };

    /**
     * uint8_t name[MGMT_MAX_NAME_LENGTH];
     * uint8_t short_name[MGMT_MAX_SHORT_NAME_LENGTH];
     */
    class MgmtEvtLocalNameChanged : public MgmtEvent
    {
        protected:
            std::string valueString() const noexcept override {
                return "name '"+getName()+"', shortName '"+getShortName()+"'";
            }

        public:
            static jau::nsize_t namesDataSize() noexcept { return MgmtConstU16::MGMT_MAX_NAME_LENGTH + MgmtConstU16::MGMT_MAX_SHORT_NAME_LENGTH; }
            static jau::nsize_t getRequiredTotalSize() noexcept { return MGMT_HEADER_SIZE + namesDataSize(); }

            MgmtEvtLocalNameChanged(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, namesDataSize())
            {
                checkOpcode(getOpcode(), Opcode::LOCAL_NAME_CHANGED);
            }
            MgmtEvtLocalNameChanged(const uint16_t dev_id, const std::string & name, const std::string & short_name)
            : MgmtEvent(Opcode::LOCAL_NAME_CHANGED, dev_id, MgmtConstU16::MGMT_MAX_NAME_LENGTH + MgmtConstU16::MGMT_MAX_SHORT_NAME_LENGTH)
            {
                pdu.put_string_nc(MGMT_HEADER_SIZE, name, MgmtConstU16::MGMT_MAX_NAME_LENGTH, true);
                pdu.put_string_nc(MGMT_HEADER_SIZE+MgmtConstU16::MGMT_MAX_NAME_LENGTH, short_name, MgmtConstU16::MGMT_MAX_SHORT_NAME_LENGTH, true);
            }

            const std::string getName() const noexcept { return pdu.get_string_nc(MGMT_HEADER_SIZE); }
            const std::string getShortName() const noexcept { return pdu.get_string_nc(MGMT_HEADER_SIZE + MgmtConstU16::MGMT_MAX_NAME_LENGTH); }

            std::shared_ptr<NameAndShortName> toNameAndShortName() const noexcept;
    };

    /**
     * uint8_t store_hint,
     * MgmtLinkKey key
     */
    class MgmtEvtNewLinkKey : public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", storeHint "+jau::to_hexstring(getStoreHint())+
                       ", "+getLinkKey().toString();
            }

        public:
            MgmtEvtNewLinkKey(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 1+sizeof(MgmtLinkKeyInfo))
            {
                checkOpcode(getOpcode(), Opcode::NEW_LINK_KEY);
            }

            uint8_t getStoreHint() const noexcept { return pdu.get_uint8_nc(MGMT_HEADER_SIZE); }

            const MgmtLinkKeyInfo& getLinkKey() const {
                return *reinterpret_cast<const MgmtLinkKeyInfo *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 1) );
            }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+1+sizeof(MgmtLinkKeyInfo); }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };

    /**
     * uint8_t store_hint,
     * MgmtLongTermKeyInfo key
     */
    class MgmtEvtNewLongTermKey : public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", store "+jau::to_hexstring(getStoreHint())+
                       ", "+getLongTermKey().toString();
            }

        public:
            MgmtEvtNewLongTermKey(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 1+sizeof(MgmtLongTermKeyInfo))
            {
                checkOpcode(getOpcode(), Opcode::NEW_LONG_TERM_KEY);
            }

            uint8_t getStoreHint() const noexcept { return pdu.get_uint8_nc(MGMT_HEADER_SIZE); }

            const MgmtLongTermKeyInfo& getLongTermKey() const {
                return *reinterpret_cast<const MgmtLongTermKeyInfo *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 1) );
            }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+1+sizeof(MgmtLongTermKeyInfo); }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * uint32_t flags,
     * uint16_t eir_len;
     * uint8_t *eir
     */
    class MgmtEvtDeviceConnected : public MgmtEvent
    {
        private:
            uint16_t hci_conn_handle;

        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address="+getAddress().toString()+
                       ", addressType "+to_string(getAddressType())+
                       ", flags="+jau::to_hexstring(getFlags())+
                       ", eir-size "+std::to_string(getEIRSize())+
                       ", hci_handle "+jau::to_hexstring(hci_conn_handle);
            }

        public:
            MgmtEvtDeviceConnected(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 13), hci_conn_handle(0xffff)
            {
                checkOpcode(getOpcode(), Opcode::DEVICE_CONNECTED);
            }
            MgmtEvtDeviceConnected(const uint16_t dev_id, const BDAddressAndType& addressAndType, const uint16_t hci_conn_handle_)
            : MgmtEvent(Opcode::DEVICE_CONNECTED, dev_id, 6+1+4+2), hci_conn_handle(hci_conn_handle_)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint32_nc(MGMT_HEADER_SIZE+6+1, 0); // flags
                pdu.put_uint16_nc(MGMT_HEADER_SIZE+6+1+4, 0); // eir-len
            }

            /** Returns the HCI connection handle, assuming creation occurred via HCIHandler */
            uint16_t getHCIHandle() const noexcept { return hci_conn_handle; }

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info

            uint32_t getFlags() const noexcept { return pdu.get_uint32_nc(MGMT_HEADER_SIZE+7); }
            uint16_t getEIRSize() const noexcept { return pdu.get_uint16_nc(MGMT_HEADER_SIZE+11); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+13; }
            jau::nsize_t getDataSize() const noexcept override { return getParamSize()-13; }
            const uint8_t* getData() const noexcept override { return getDataSize()>0 ? pdu.get_ptr_nc(getDataOffset()) : nullptr; }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * uint8_t reason
     */
    class MgmtEvtDeviceDisconnected : public MgmtEvent
    {
        public:
            enum class DisconnectReason : uint8_t {
                UNKNOWN        = 0x00,
                TIMEOUT        = 0x01,
                LOCAL_HOST     = 0x02,
                REMOTE         = 0x03,
                AUTH_FAILURE   = 0x04
            };
            static std::string getDisconnectReasonString(DisconnectReason mgmtReason) noexcept;

            /**
             * BlueZ Kernel Mgmt has reduced information by HCIStatusCode -> DisconnectReason,
             * now the inverse surely can't repair this loss.
             * <p>
             * See getDisconnectReason(HCIStatusCode) below for the mentioned mapping.
             * </p>
             */
            static HCIStatusCode getHCIReason(DisconnectReason mgmtReason) noexcept;

            /**
             * BlueZ Kernel Mgmt mapping of HCI disconnect reason, which reduces some information.
             */
            static DisconnectReason getDisconnectReason(HCIStatusCode hciReason) noexcept;

        private:
            const HCIStatusCode hciReason;
            const uint16_t hci_conn_handle;

        protected:
            std::string baseString() const noexcept override {
                const DisconnectReason reason1 = getReason();
                const HCIStatusCode reason2 = getHCIReason();
                return MgmtEvent::baseString()+", address="+getAddress().toString()+
                       ", addressType "+to_string(getAddressType())+
                       ", reason[mgmt["+jau::to_hexstring(static_cast<uint8_t>(reason1))+" ("+getDisconnectReasonString(reason1)+")]"+
                       ", hci["+jau::to_hexstring(static_cast<uint8_t>(reason2))+" ("+to_string(reason2)+")]]"+
                       ", hci_handle "+jau::to_hexstring(hci_conn_handle);
            }

        public:
            MgmtEvtDeviceDisconnected(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 8), hciReason(HCIStatusCode::UNKNOWN), hci_conn_handle(0xffff)
            {
                checkOpcode(getOpcode(), Opcode::DEVICE_DISCONNECTED);
            }
            MgmtEvtDeviceDisconnected(const uint16_t dev_id, const BDAddressAndType& addressAndType,
                                      HCIStatusCode hciReason_, const uint16_t hci_conn_handle_)
            : MgmtEvent(Opcode::DEVICE_DISCONNECTED, dev_id, 6+1+1), hciReason(hciReason_), hci_conn_handle(hci_conn_handle_)
            {
                DisconnectReason disconnectReason = getDisconnectReason(hciReason_);
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6+1, static_cast<uint8_t>(disconnectReason));
            }

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info

            DisconnectReason getReason() const noexcept { return static_cast<DisconnectReason>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+7)); }

            /** Returns either the HCI reason if given, or the translated DisconnectReason. */
            HCIStatusCode getHCIReason() const noexcept {
                if( HCIStatusCode::UNKNOWN != hciReason ) {
                    return hciReason;
                }
                return getHCIReason(getReason());
            }

            /** Returns the disconnected HCI connection handle, assuming creation occurred via HCIHandler */
            uint16_t getHCIHandle() const noexcept { return hci_conn_handle; }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+8; }
            jau::nsize_t getDataSize() const noexcept override { return getParamSize()-8; }
            const uint8_t* getData() const noexcept override { return getDataSize()>0 ? pdu.get_ptr_nc(getDataOffset()) : nullptr; }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * uint8_t status
     */
    class MgmtEvtDeviceConnectFailed : public MgmtEvent
    {
        private:
            const HCIStatusCode hciStatus;

        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address="+getAddress().toString()+
                       ", addressType "+to_string(getAddressType())+
                       ", status[mgmt["+jau::to_hexstring(static_cast<uint8_t>(getStatus()))+" ("+to_string(getStatus())+")]"+
                       ", hci["+jau::to_hexstring(static_cast<uint8_t>(hciStatus))+" ("+to_string(hciStatus)+")]]";
            }

        public:
            MgmtEvtDeviceConnectFailed(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 8), hciStatus(HCIStatusCode::UNKNOWN)
            {
                checkOpcode(getOpcode(), Opcode::CONNECT_FAILED);
            }
            MgmtEvtDeviceConnectFailed(const uint16_t dev_id, const BDAddressAndType& addressAndType, const HCIStatusCode status)
            : MgmtEvent(Opcode::CONNECT_FAILED, dev_id, 6+1+1), hciStatus(status)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6+1, static_cast<uint8_t>(MgmtStatus::CONNECT_FAILED));
            }
            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info

            MgmtStatus getStatus() const noexcept { return static_cast<MgmtStatus>( pdu.get_uint8_nc(MGMT_HEADER_SIZE+7) ); }

            /** Return the root reason in non reduced HCIStatusCode space, if available. Otherwise this value will be HCIStatusCode::UNKNOWN. */
            HCIStatusCode getHCIStatus() const noexcept { return hciStatus; }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+8; }
            jau::nsize_t getDataSize() const noexcept override { return getParamSize()-8; }
            const uint8_t* getData() const noexcept override { return getDataSize()>0 ? pdu.get_ptr_nc(getDataOffset()) : nullptr; }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * uint8_t secure
     */
    class MgmtEvtPinCodeRequest : public MgmtEvent
    {
        public:

        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address="+getAddress().toString()+
                       ", addressType "+to_string(getAddressType())+
                       ", secure "+std::to_string(getSecure());
            }

        public:
            MgmtEvtPinCodeRequest(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 8)
            {
                checkOpcode(getOpcode(), Opcode::PIN_CODE_REQUEST);
            }
            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info

            uint8_t getSecure() const noexcept { return pdu.get_uint8_nc(MGMT_HEADER_SIZE+7); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+8; }
            jau::nsize_t getDataSize() const noexcept override { return getParamSize()-8; }
            const uint8_t* getData() const noexcept override { return getDataSize()>0 ? pdu.get_ptr_nc(getDataOffset()) : nullptr; }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * uint8_t confirm_hint
     * uint32_t value
     */
    class MgmtEvtUserConfirmRequest: public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address["+getAddress().toString()+
                       ", "+to_string(getAddressType())+
                       "], confirm_hint "+jau::to_hexstring(getConfirmHint())+", value "+jau::to_hexstring(getValue());
            }

        public:
            MgmtEvtUserConfirmRequest(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 6+1+1+4)
            {
                checkOpcode(getOpcode(), Opcode::USER_CONFIRM_REQUEST);
            }

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info

            uint8_t getConfirmHint() const noexcept { return pdu.get_uint8_nc(MGMT_HEADER_SIZE+6+1); }
            uint32_t getValue() const noexcept { return pdu.get_uint32_nc(MGMT_HEADER_SIZE+6+1+1); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+6+1+1+4; }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtEvtUserPasskeyRequest: public MgmtEvtAdressInfoMeta
    {
        public:
            MgmtEvtUserPasskeyRequest(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvtAdressInfoMeta(Opcode::USER_PASSKEY_REQUEST, buffer, buffer_len)
            { }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * uint8_t status
     */
    class MgmtEvtAuthFailed: public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address["+getAddress().toString()+
                       ", "+to_string(getAddressType())+
                       "], status "+to_string(getStatus());
            }
        public:
            MgmtEvtAuthFailed(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 6+1+1)
            {
                checkOpcode(getOpcode(), Opcode::AUTH_FAILED);
            }

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info

            MgmtStatus getStatus() const noexcept { return static_cast<MgmtStatus>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6+1)); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+7; }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * int8_t rssi,
     * uint32_t flags,
     * uint16_t eir_len;
     * uint8_t *eir
     */
    class MgmtEvtDeviceFound : public MgmtEvent
    {
        private:
            std::unique_ptr<EInfoReport> eireport;

        protected:
            std::string baseString() const noexcept override {
                if( nullptr != eireport ) {
                    return MgmtEvent::baseString()+", "+eireport->toString(false /* includeServices */);
                } else {
                    return MgmtEvent::baseString()+", address="+getAddress().toString()+
                           ", addressType "+to_string(getAddressType())+
                           ", rssi "+std::to_string(getRSSI())+", flags="+jau::to_hexstring(getFlags())+
                           ", eir-size "+std::to_string(getEIRSize());
                }
            }

        public:
            MgmtEvtDeviceFound(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 14), eireport(nullptr)
            {
                checkOpcode(getOpcode(), Opcode::DEVICE_FOUND);
            }
            MgmtEvtDeviceFound(const uint16_t dev_id, std::unique_ptr<EInfoReport> && eir)
            : MgmtEvent(Opcode::DEVICE_FOUND, dev_id, 6+1+1+4+2+0), eireport(std::move(eir))
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, eireport->getAddress());
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(eireport->getAddressType()));
                pdu.put_int8_nc(MGMT_HEADER_SIZE+6+1, eireport->getRSSI());
                pdu.put_uint32_nc(MGMT_HEADER_SIZE+6+1+1, direct_bt::number(eireport->getFlags())); // EIR flags only 8bit, Mgmt uses 32bit?
                pdu.put_uint16_nc(MGMT_HEADER_SIZE+6+1+1+4, 0); // eir_len
            }

            /** Returns reference to the immutable EInfoReport, assuming creation occurred via HCIHandler. Otherwise nullptr. */
            const EInfoReport* getEIR() const noexcept { return eireport.get(); }

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info

            int8_t getRSSI() const noexcept { return pdu.get_int8_nc(MGMT_HEADER_SIZE+7); }
            uint32_t getFlags() const noexcept { return pdu.get_uint32_nc(MGMT_HEADER_SIZE+8); }
            uint16_t getEIRSize() const noexcept { return pdu.get_uint16_nc(MGMT_HEADER_SIZE+12); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+14; }
            jau::nsize_t getDataSize() const noexcept override { return getParamSize()-14; }
            const uint8_t* getData() const noexcept override { return getDataSize()>0 ? pdu.get_ptr_nc(getDataOffset()) : nullptr; }
    };

    class MgmtEvtDiscovering : public MgmtEvent
    {
        public:

        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", scan-type "+to_string(getScanType())+
                       ", enabled "+std::to_string(getEnabled());
            }

        public:
            MgmtEvtDiscovering(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 2)
            {
                checkOpcode(getOpcode(), Opcode::DISCOVERING);
            }

            MgmtEvtDiscovering(const uint16_t dev_id, const ScanType scanType, const bool enabled)
            : MgmtEvent(Opcode::DISCOVERING, dev_id, 1+1)
            {
                pdu.put_uint8_nc(MGMT_HEADER_SIZE, direct_bt::number(scanType));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+1, enabled);
            }

            ScanType getScanType() const noexcept { return static_cast<ScanType>( pdu.get_uint8_nc(MGMT_HEADER_SIZE) ); }
            bool getEnabled() const noexcept { return 0 != pdu.get_uint8_nc(MGMT_HEADER_SIZE+1); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+2; }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtEvtDeviceBlocked : public MgmtEvtAdressInfoMeta
    {
        public:
            MgmtEvtDeviceBlocked(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvtAdressInfoMeta(Opcode::DEVICE_BLOCKED, buffer, buffer_len)
            { }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtEvtDeviceUnblocked : public MgmtEvtAdressInfoMeta
    {
        public:
            MgmtEvtDeviceUnblocked(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvtAdressInfoMeta(Opcode::DEVICE_UNBLOCKED, buffer, buffer_len)
            { }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtEvtDeviceUnpaired : public MgmtEvtAdressInfoMeta
    {
        public:
            MgmtEvtDeviceUnpaired(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvtAdressInfoMeta(Opcode::DEVICE_UNPAIRED, buffer, buffer_len)
            { }
    };

    // TODO PASSKEY_NOTIFY             = 0x0017,

    /**
     * uint8_t store_hint,
     * EUI48 random_address;
     * MgmtIdentityResolvingKey key
     */
    class MgmtEvtNewIdentityResolvingKey : public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", store "+jau::to_hexstring(getStoreHint())+
                       ", rnd_address "+getRandomAddress().toString()+
                       +", "+getIdentityResolvingKey().toString();
            }

        public:
            MgmtEvtNewIdentityResolvingKey(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 1+6+sizeof(MgmtIdentityResolvingKeyInfo))
            {
                checkOpcode(getOpcode(), Opcode::NEW_IRK);
            }

            uint8_t getStoreHint() const noexcept { return pdu.get_uint8_nc(MGMT_HEADER_SIZE); }

            const EUI48& getRandomAddress() const { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 1) ); }

            const MgmtIdentityResolvingKeyInfo& getIdentityResolvingKey() const {
                return *reinterpret_cast<const MgmtIdentityResolvingKeyInfo *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 1 + 6) );
            }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+1+sizeof(MgmtIdentityResolvingKeyInfo); }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };

    /**
     * uint8_t store_hint,
     * EUI48 random_address;
     * MgmtSignatureResolvingKeyInfo key
     */
    class MgmtEvtNewSignatureResolvingKey : public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", store "+jau::to_hexstring(getStoreHint())+
                       +", "+getSignatureResolvingKey().toString();
            }

        public:
            MgmtEvtNewSignatureResolvingKey(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 1+sizeof(MgmtSignatureResolvingKeyInfo))
            {
                checkOpcode(getOpcode(), Opcode::NEW_CSRK);
            }

            uint8_t getStoreHint() const noexcept { return pdu.get_uint8_nc(MGMT_HEADER_SIZE); }

            const MgmtSignatureResolvingKeyInfo& getSignatureResolvingKey() const {
                return *reinterpret_cast<const MgmtSignatureResolvingKeyInfo *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 1) );
            }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+1+sizeof(MgmtSignatureResolvingKeyInfo); }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };


    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * uint8_t action
     */
    class MgmtEvtDeviceWhitelistAdded : public MgmtEvent
    {
        public:

        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address="+getAddress().toString()+
                       ", addressType "+to_string(getAddressType())+
                       ", action "+std::to_string(getAction());
            }

        public:
            MgmtEvtDeviceWhitelistAdded(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 8)
            {
                checkOpcode(getOpcode(), Opcode::DEVICE_WHITELIST_ADDED);
            }
            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info

            uint8_t getAction() const noexcept { return pdu.get_uint8_nc(MGMT_HEADER_SIZE+7); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+8; }
            jau::nsize_t getDataSize() const noexcept override { return getParamSize()-8; }
            const uint8_t* getData() const noexcept override { return getDataSize()>0 ? pdu.get_ptr_nc(getDataOffset()) : nullptr; }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     */
    class MgmtEvtDeviceWhitelistRemoved : public MgmtEvtAdressInfoMeta
    {
        public:
            MgmtEvtDeviceWhitelistRemoved(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvtAdressInfoMeta(Opcode::DEVICE_WHITELIST_REMOVED, buffer, buffer_len)
            { }
    };

    /**
     * int8_t store_hint,
     * MgmtConnParam connParam
     */
    class MgmtEvtNewConnectionParam : public MgmtEvent
    {
        public:

        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+
                       ", store "+jau::to_hexstring(getStoreHint())+
                       ", "+getConnParam().toString();
            }

        public:
            MgmtEvtNewConnectionParam(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(buffer, buffer_len, 1 + sizeof(MgmtConnParam))
            {
                checkOpcode(getOpcode(), Opcode::NEW_CONN_PARAM);
            }

            uint8_t getStoreHint() const noexcept { return pdu.get_int8_nc(MGMT_HEADER_SIZE); }

            const MgmtConnParam& getConnParam() const {
                return *reinterpret_cast<const MgmtConnParam *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE+1) );
            }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+16; }
            jau::nsize_t getDataSize() const noexcept override { return getParamSize()-16; }
            const uint8_t* getData() const noexcept override { return getDataSize()>0 ? pdu.get_ptr_nc(getDataOffset()) : nullptr; }
    };

    // TODO UNCONF_INDEX_ADDED         = 0x001D,
    // TODO UNCONF_INDEX_REMOVED       = 0x001E,
    // TODO NEW_CONFIG_OPTIONS         = 0x001F,
    // TODO EXT_INDEX_ADDED            = 0x0020,
    // TODO EXT_INDEX_REMOVED          = 0x0021,
    // TODO LOCAL_OOB_DATA_UPDATED     = 0x0022,
    // TODO ADVERTISING_ADDED          = 0x0023,
    // TODO ADVERTISING_REMOVED        = 0x0024,
    // TODO EXT_INFO_CHANGED           = 0x0025,
    // TODO PHY_CONFIGURATION_CHANGED  = 0x0026,
    // TODO MGMT_EVENT_TYPE_COUNT      = 0x0026

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * MgmtStatus (1 octet)
     */
    class MgmtEvtPairDeviceComplete : public MgmtEvent
    {
        protected:
        std::string baseString() const noexcept override {
            return MgmtEvent::baseString()+", address="+getAddress().toString()+
                   ", addressType "+to_string(getAddressType())+
                   ", status "+to_string(getStatus());
        }

        public:
            static jau::nsize_t getRequiredTotalSize() noexcept { return MGMT_HEADER_SIZE + 3 + 6 + 1; }

            /** Converts MgmtEvtCmdComplete -> MgmtEvtPairDeviceComplete */
            MgmtEvtPairDeviceComplete(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvent(Opcode::PAIR_DEVICE_COMPLETE, MgmtMsg::getDevID(buffer), 6+1+1)
            {
                (void)buffer_len;
                const MgmtStatus status = MgmtEvtCmdComplete::getStatus(buffer);
                const EUI48& address = *reinterpret_cast<const EUI48 *>( buffer + MGMT_HEADER_SIZE + 3 + 0 ); // mgmt_addr_info
                const BDAddressType addressType = static_cast<BDAddressType>( jau::get_uint8(buffer, MGMT_HEADER_SIZE + 3 + 6) ); // mgmt_addr_info

                pdu.put_eui48_nc(MGMT_HEADER_SIZE, address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressType));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6+1, static_cast<uint8_t>(status));
            }

            MgmtEvtPairDeviceComplete(const uint16_t dev_id, const EUI48& address, const BDAddressType addressType, const MgmtStatus status)
            : MgmtEvent(Opcode::PAIR_DEVICE_COMPLETE, dev_id, 6+1+1)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressType));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6+1, static_cast<uint8_t>(status));
            }

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE + 6)); } // mgmt_addr_info

            MgmtStatus getStatus() const noexcept { return static_cast<MgmtStatus>(pdu.get_uint8_nc(MGMT_HEADER_SIZE + 6 + 1)); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+6+1+1; }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * uint64_t features (8 Octets)
     *
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.65.4 LE Read Remote Features Complete event
     *
     * <p>
     * This is a Direct_BT extension for HCI.
     * </p>
     */
    class MgmtEvtHCILERemoteFeatures : public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address="+getAddress().toString()+
                       ", addressType "+to_string(getAddressType())+
                       ", status "+to_string(getHCIStatus())+
                       ", features="+jau::to_hexstring(direct_bt::number(getFeatures()));
            }

        public:
            MgmtEvtHCILERemoteFeatures(const uint16_t dev_id, const BDAddressAndType& addressAndType, const HCIStatusCode hci_status, const LE_Features features_)
            : MgmtEvent(Opcode::HCI_LE_REMOTE_FEATURES, dev_id, 6+1+8)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6+1, direct_bt::number(hci_status));
                pdu.put_uint64_nc(MGMT_HEADER_SIZE+6+1+1, direct_bt::number(features_));
            }

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info
            HCIStatusCode getHCIStatus() const noexcept { return static_cast<HCIStatusCode>( pdu.get_uint8_nc(MGMT_HEADER_SIZE+6+1) ); }

            LE_Features getFeatures() const noexcept { return static_cast<LE_Features>(pdu.get_uint64_nc(MGMT_HEADER_SIZE+6+1+1)); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+6+1+1+8; }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * uint8_t Tx (8 Octets)
     * uint8_t Rx (8 Octets)
     *
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.65.12 LE PHY Update Complete event
     *
     * <p>
     * This is a Direct_BT extension for HCI.
     * </p>
     */
    class MgmtEvtHCILEPhyUpdateComplete : public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address="+getAddress().toString()+
                       ", addressType "+to_string(getAddressType())+
                       ", status "+to_string(getHCIStatus())+
                       ", Tx="+direct_bt::to_string(getTx())+
                       ", Rx="+direct_bt::to_string(getRx());
            }

        public:
            MgmtEvtHCILEPhyUpdateComplete(const uint16_t dev_id, const BDAddressAndType& addressAndType, const HCIStatusCode hci_status, const LE_PHYs Tx, const LE_PHYs Rx)
            : MgmtEvent(Opcode::HCI_LE_PHY_UPDATE_COMPLETE, dev_id, 6+1+2)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6+1, direct_bt::number(hci_status));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6+1+1, direct_bt::number(Tx));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6+1+1+1, direct_bt::number(Rx));
            }

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info
            HCIStatusCode getHCIStatus() const noexcept { return static_cast<HCIStatusCode>( pdu.get_uint8_nc(MGMT_HEADER_SIZE+6+1) ); }

            LE_PHYs getTx() const noexcept { return static_cast<LE_PHYs>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6+1+1)); }
            LE_PHYs getRx() const noexcept { return static_cast<LE_PHYs>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6+1+1+1)); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+6+1+1+1+1; }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.65.5 LE Long Term Key Request event
     *
     * - mgmt_addr_info { EUI48, uint8_t type },
     * - uint64_t random_number (8 octets)
     * - uint16_t ediv (2 octets)
     *
     * This event indicates that the peer device being BTRole::Master, attempts to encrypt or re-encrypt the link
     * and is requesting the LTK from the Host.
     *
     * This event shall only be generated when the local device’s role is BTRole::Slave (responder, adapter in peripheral mode).
     *
     * Rand and Ediv belong to the local device having role BTRole::Slave (responder).
     *
     * Rand and Ediv matches the LTK from SMP messaging in SC mode only!
     * <p>
     * This is a Direct_BT extension for HCI.
     * </p>
     */
    class MgmtEvtHCILELTKReq : public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address="+getAddress().toString()+
                       ", addressType "+to_string(getAddressType())+
                        ", rand "+jau::bytesHexString(pdu.get_ptr_nc(MGMT_HEADER_SIZE), 6+1,   8, false /* lsbFirst */)+
                        ", ediv "+jau::bytesHexString(pdu.get_ptr_nc(MGMT_HEADER_SIZE), 6+1+8, 2, false /* lsbFirst */);
            }
        public:
            MgmtEvtHCILELTKReq(const uint16_t dev_id, const BDAddressAndType& addressAndType, const uint64_t rand, const uint16_t ediv)
            : MgmtEvent(Opcode::HCI_LE_LTK_REQUEST, dev_id, 6+1+8+2)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint64_nc(MGMT_HEADER_SIZE+6+1, rand);
                pdu.put_uint16_nc(MGMT_HEADER_SIZE+6+1+8, ediv);
            }

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info

            /**
             * Returns the 64-bit Rand value (8 octets) being distributed
             * <p>
             * See Vol 3, Part H, 2.4.2.3 SM - Generation of CSRK - LE legacy pairing - generation of LTK, EDIV and Rand.
             * </p>
             */
            constexpr uint64_t getRand() const noexcept { return pdu.get_uint64_nc(MGMT_HEADER_SIZE+6+1); }

            /**
             * Returns the 16-bit EDIV value (2 octets) being distributed
             * <p>
             * See Vol 3, Part H, 2.4.2.3 SM - Generation of CSRK - LE legacy pairing - generation of LTK, EDIV and Rand.
             * </p>
             */
            constexpr uint16_t getEDIV() const noexcept { return pdu.get_uint16_nc(MGMT_HEADER_SIZE+6+1+8); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+6+1+8+2; }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }

            /**
             * Convert this instance into its platform agnostic SMPLongTermKeyInfo type,
             * invalid without LTK.
             *
             * LTK shall be completed via MgmtEvtHCILELTKReplyAckCmd.
             *
             * Local device’s role is BTRole::Slave, responder.
             */
            SMPLongTermKey toSMPLongTermKeyInfo(const bool isSC, const bool isAuth) const noexcept {
                direct_bt::SMPLongTermKey res;
                res.clear();
                res.properties |= SMPLongTermKey::Property::RESPONDER;
                if( isSC ) {
                    res.properties |= SMPLongTermKey::Property::SC;
                }
                if( isAuth ) {
                    res.properties |= SMPLongTermKey::Property::AUTH;
                }
                res.enc_size = 0; // not yet valid;
                res.ediv = getEDIV();
                res.rand = getRand();
                // res.ltk = getLTK(); // -> MgmtEvtHCILELTKReplyAckCmd
                return res;
            }
    };

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.25 LE Long Term Key Request Reply command
     *
     * - mgmt_addr_info { EUI48, uint8_t type },
     * - uint128_t ltk (16 octets)
     *
     * This command shall only be used when the local device’s role is BTRole::Slave (responder).
     *
     * LTK belongs to the local device having role BTRole::Slave (responder).
     *
     * The encryption key matches the LTK from SMP messaging in SC mode only!
     * <p>
     * This is a Direct_BT extension for HCI.
     * </p>
     */
    class MgmtEvtHCILELTKReplyAckCmd : public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address="+getAddress().toString()+
                       ", addressType "+to_string(getAddressType())+
                       ", ltk "+jau::bytesHexString(pdu.get_ptr_nc(MGMT_HEADER_SIZE), 6+1, 16, true /* lsbFirst */);
            }
        public:
            MgmtEvtHCILELTKReplyAckCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType, const jau::uint128_t ltk)
            : MgmtEvent(Opcode::HCI_LE_LTK_REPLY_ACK, dev_id, 6+1+16)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint128_nc(MGMT_HEADER_SIZE+6+1, ltk);
            }

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info

            /**
             * Returns the 128-bit Long Term Key (16 octets)
             * <p>
             * The generated LTK value being distributed,
             * see Vol 3, Part H, 2.4.2.3 SM - LE legacy pairing - generation of LTK, EDIV and Rand.
             * </p>
             */
            constexpr jau::uint128_t getLTK() const noexcept { return pdu.get_uint128_nc(MGMT_HEADER_SIZE+6+1); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+6+1+16; }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.26 LE Long Term Key Request Negative Reply command
     *
     * - mgmt_addr_info { EUI48, uint8_t type },
     *
     * <p>
     * This is a Direct_BT extension for HCI.
     * </p>
     */
    class MgmtEvtHCILELTKReplyRejCmd : public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address="+getAddress().toString()+
                       ", addressType "+to_string(getAddressType());
            }

        public:
            MgmtEvtHCILELTKReplyRejCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType)
            : MgmtEvent(Opcode::HCI_LE_LTK_REPLY_REJ, dev_id, 6+1)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
            }

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+6+1; }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8.24 LE Enable Encryption command
     *
     * - mgmt_addr_info { EUI48, uint8_t type },
     * - uint64_t random_number (8 octets)
     * - uint16_t ediv (2 octets)
     * - uint128_t ltk (16 octets)
     *
     * Controller replies to this command with HCI_Command_Status event to the Host.
     * - If the connection wasn't encrypted yet, HCI_Encryption_Change event shall occur when encryption has been started.
     * - Otherwise HCI_Encryption_Key_Refresh_Complete event shall occur when encryption has been resumed.
     *
     * This command shall only be used when the local device’s role is BTRole::Master (initiator).
     *
     * Encryption key belongs to the remote device having role BTRole::Slave (responder).
     *
     * The encryption key matches the LTK from SMP messaging in SC mode only!
     * <p>
     * This is a Direct_BT extension for HCI.
     * </p>
     */
    class MgmtEvtHCILEEnableEncryptionCmd : public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address="+getAddress().toString()+
                       ", addressType "+to_string(getAddressType())+
                       ", rand "+jau::bytesHexString(pdu.get_ptr_nc(MGMT_HEADER_SIZE), 6+1,   8, false /* lsbFirst */)+
                       ", ediv "+jau::bytesHexString(pdu.get_ptr_nc(MGMT_HEADER_SIZE), 6+1+8, 2, false /* lsbFirst */)+
                       ", ltk "+jau::bytesHexString(pdu.get_ptr_nc(MGMT_HEADER_SIZE), 6+1+8+2, 16, true /* lsbFirst */);
            }

        public:
            MgmtEvtHCILEEnableEncryptionCmd(const uint16_t dev_id, const BDAddressAndType& addressAndType,
                                            const uint64_t rand, const uint16_t ediv, const jau::uint128_t ltk)
            : MgmtEvent(Opcode::HCI_LE_ENABLE_ENC, dev_id, 6+1+8+2+16)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint64_nc(MGMT_HEADER_SIZE+6+1, rand);
                pdu.put_uint16_nc(MGMT_HEADER_SIZE+6+1+8, ediv);
                pdu.put_uint128_nc(MGMT_HEADER_SIZE+6+1+8+2, ltk);
            }

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info

            /**
             * Returns the 64-bit Rand value (8 octets) being distributed
             * <p>
             * See Vol 3, Part H, 2.4.2.3 SM - Generation of CSRK - LE legacy pairing - generation of LTK, EDIV and Rand.
             * </p>
             */
            constexpr uint64_t getRand() const noexcept { return pdu.get_uint64_nc(MGMT_HEADER_SIZE+6+1); }

            /**
             * Returns the 16-bit EDIV value (2 octets) being distributed
             * <p>
             * See Vol 3, Part H, 2.4.2.3 SM - Generation of CSRK - LE legacy pairing - generation of LTK, EDIV and Rand.
             * </p>
             */
            constexpr uint16_t getEDIV() const noexcept { return pdu.get_uint16_nc(MGMT_HEADER_SIZE+6+1+8); }

            /**
             * Returns the 128-bit Long Term Key (16 octets)
             * <p>
             * The generated LTK value being distributed,
             * see Vol 3, Part H, 2.4.2.3 SM - LE legacy pairing - generation of LTK, EDIV and Rand.
             * </p>
             */
            constexpr jau::uint128_t getLTK() const noexcept { return pdu.get_uint128_nc(MGMT_HEADER_SIZE+6+1+8+2); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+6+1+8+2+16; }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }

            /**
             * Convert this instance into its platform agnostic SMPLongTermKeyInfo LTK.
             *
             * Local device’s role is BTRole::Master, initiator.
             *
             * This LTK Encryption key is for the remote device having role BTRole::Slave (responder).
             */
            SMPLongTermKey toSMPLongTermKeyInfo(const bool isSC, const bool isAuth) const noexcept {
                direct_bt::SMPLongTermKey res;
                res.clear();
                res.properties |= SMPLongTermKey::Property::RESPONDER;
                if( isSC ) {
                    res.properties |= SMPLongTermKey::Property::SC;
                }
                if( isAuth ) {
                    res.properties |= SMPLongTermKey::Property::AUTH;
                }
                res.enc_size = 16;
                res.ediv = getEDIV();
                res.rand = getRand();
                res.ltk = getLTK();
                return res;
            }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * HCIStatusCode status (1 Octet)
     * uint8_t enc_enabled (1 Octet)
     * <p>
     * On BTRole::Master (reply to MgmtEvtHCILEEnableEncryptionCmd) and BTRole::Slave
     * </p>
     * <p>
     * This is a Direct_BT extension for HCI.
     * </p>
     * <pre>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.8 HCIEventType::ENCRYPT_CHANGE
     * </pre>
     */
    class MgmtEvtHCIEncryptionChanged : public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address="+getAddress().toString()+
                       ", addressType "+to_string(getAddressType())+
                       ", status "+to_string(getHCIStatus())+
                       ", enabled "+jau::to_hexstring(getEncEnabled());
            }

        public:
            MgmtEvtHCIEncryptionChanged(const uint16_t dev_id, const BDAddressAndType& addressAndType, const HCIStatusCode hci_status, uint8_t hci_enc_enabled)
            : MgmtEvent(Opcode::HCI_ENC_CHANGED, dev_id, 6+1+1+1)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6+1, direct_bt::number(hci_status));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6+1+1, hci_enc_enabled);
            }

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info
            HCIStatusCode getHCIStatus() const noexcept { return static_cast<HCIStatusCode>( pdu.get_uint8_nc(MGMT_HEADER_SIZE+6+1) ); }
            uint8_t getEncEnabled() const noexcept { return pdu.get_uint8_nc(MGMT_HEADER_SIZE+6+1+1); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+1+2+1; }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };

    /**
     * mgmt_addr_info { EUI48, uint8_t type },
     * HCIStatusCode status (1 Octet)
     * <p>
     * On BTRole::Master (reply to MgmtEvtHCILEEnableEncryptionCmd) and BTRole::Slave
     * </p>
     * <p>
     * This is a Direct_BT extension for HCI.
     * </p>
     * <pre>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.39 HCIEventType::ENCRYPT_KEY_REFRESH_COMPLETE
     * </pre>
     */
    class MgmtEvtHCIEncryptionKeyRefreshComplete : public MgmtEvent
    {
        protected:
            std::string baseString() const noexcept override {
                return MgmtEvent::baseString()+", address="+getAddress().toString()+
                       ", addressType "+to_string(getAddressType())+
                       ", status "+to_string(getHCIStatus());
            }

        public:
            MgmtEvtHCIEncryptionKeyRefreshComplete(const uint16_t dev_id, const BDAddressAndType& addressAndType, const HCIStatusCode hci_status)
            : MgmtEvent(Opcode::HCI_ENC_KEY_REFRESH_COMPLETE, dev_id, 6+1+1)
            {
                pdu.put_eui48_nc(MGMT_HEADER_SIZE, addressAndType.address);
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6, direct_bt::number(addressAndType.type));
                pdu.put_uint8_nc(MGMT_HEADER_SIZE+6+1, direct_bt::number(hci_status));
            }

            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(MGMT_HEADER_SIZE + 0) ); } // mgmt_addr_info
            BDAddressType getAddressType() const noexcept { return static_cast<BDAddressType>(pdu.get_uint8_nc(MGMT_HEADER_SIZE+6)); } // mgmt_addr_info
            HCIStatusCode getHCIStatus() const noexcept { return static_cast<HCIStatusCode>( pdu.get_uint8_nc(MGMT_HEADER_SIZE+6+1) ); }

            jau::nsize_t getDataOffset() const noexcept override { return MGMT_HEADER_SIZE+1+2+1; }
            jau::nsize_t getDataSize() const noexcept override { return 0; }
            const uint8_t* getData() const noexcept override { return nullptr; }
    };

    class MgmtEvtAdapterInfo : public MgmtEvtCmdComplete
    {
        protected:
            std::string valueString() const noexcept override {
                return getAddress().toString()+", version "+std::to_string(getVersion())+
                        ", manuf "+std::to_string(getManufacturer())+
                        ", settings[sup "+to_string(getSupportedSetting())+", cur "+to_string(getCurrentSetting())+
                        "], name '"+getName()+"', shortName '"+getShortName()+"'";
            }

        public:
            static jau::nsize_t infoDataSize() noexcept { return 20 + MgmtConstU16::MGMT_MAX_NAME_LENGTH + MgmtConstU16::MGMT_MAX_SHORT_NAME_LENGTH; }
            static jau::nsize_t getRequiredTotalSize() noexcept { return MGMT_HEADER_SIZE + 3 + infoDataSize(); }

            MgmtEvtAdapterInfo(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : MgmtEvtCmdComplete(buffer, buffer_len, infoDataSize())
            { }

            /** The adapter address reported is always the public address, i.e. BDAddressType::BDADDR_LE_PUBLIC. */
            const EUI48& getAddress() const noexcept { return *reinterpret_cast<const EUI48 *>( pdu.get_ptr_nc(getDataOffset() + 0) ); }
            uint8_t getVersion() const noexcept { return pdu.get_uint8_nc(getDataOffset()+6); }
            uint16_t getManufacturer() const noexcept { return pdu.get_uint16_nc(getDataOffset()+7); }
            AdapterSetting getSupportedSetting() const noexcept { return static_cast<AdapterSetting>( pdu.get_uint32_nc(getDataOffset()+9) ); }
            AdapterSetting getCurrentSetting() const noexcept { return static_cast<AdapterSetting>( pdu.get_uint32_nc(getDataOffset()+13) ); }
            uint32_t getDevClass() const noexcept { return pdu.get_uint8_nc(getDataOffset()+17)
                                                       | ( pdu.get_uint8_nc(getDataOffset()+18) << 8 )
                                                       | ( pdu.get_uint8_nc(getDataOffset()+19) << 16 ); }
            std::string getName() const noexcept { return pdu.get_string_nc(getDataOffset()+20); }
            std::string getShortName() const noexcept { return pdu.get_string_nc(getDataOffset()+20+MgmtConstU16::MGMT_MAX_NAME_LENGTH); }

            std::unique_ptr<AdapterInfo> toAdapterInfo() const noexcept;
            bool updateAdapterInfo(AdapterInfo& info) const noexcept;
    };

    typedef jau::FunctionDef<bool, const MgmtEvent&> MgmtEventCallback;
    typedef jau::cow_darray<MgmtEventCallback> MgmtEventCallbackList;

    class MgmtAdapterEventCallback {
        private:
            /** Unique adapter index filter or <code>-1</code> to listen for all adapter. */
            int dev_id;
            /** Documented the related callback Opcode . */
            MgmtEvent::Opcode opc;
            /** MgmtEventCallback instance */
            MgmtEventCallback callback;

        public:
            MgmtAdapterEventCallback(const int _dev_id, const MgmtEvent::Opcode _opc, const MgmtEventCallback & _callback) noexcept
            : dev_id(_dev_id), opc(_opc), callback(_callback) {}

            MgmtAdapterEventCallback(const MgmtAdapterEventCallback &o) noexcept = default;
            MgmtAdapterEventCallback(MgmtAdapterEventCallback &&o) noexcept = default;
            MgmtAdapterEventCallback& operator=(const MgmtAdapterEventCallback &o) noexcept = default;
            MgmtAdapterEventCallback& operator=(MgmtAdapterEventCallback &&o) noexcept = default;

            /** Unique adapter index filter or <code>-1</code> to listen for all adapter. */
            int getDevID() const noexcept { return dev_id; }

            /** MgmtEventCallback reference */
            MgmtEventCallback& getCallback() noexcept { return callback; }

            /** const MgmtEventCallback reference */
            const MgmtEventCallback& getCallback() const noexcept { return callback; }

            bool operator==(const MgmtAdapterEventCallback& rhs) const noexcept
            { return dev_id == rhs.dev_id && callback == rhs.callback; }

            bool operator!=(const MgmtAdapterEventCallback& rhs) const noexcept
            { return !(*this == rhs); }

            std::string toString() const {
                return "MgmtAdapterEventCallback[dev_id "+std::to_string(dev_id)+", "+MgmtEvent::getOpcodeString(opc)+", "+callback.toString()+"]";
            }
    };

    typedef jau::cow_darray<MgmtAdapterEventCallback> MgmtAdapterEventCallbackList;

} // namespace direct_bt

#endif /* MGMT_TYPES_HPP_ */
