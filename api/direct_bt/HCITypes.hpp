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

#ifndef HCI_TYPES_HPP_
#define HCI_TYPES_HPP_

#include <cstring>
#include <string>
#include <cstdint>

#include <algorithm>
#include <mutex>

#include <jau/basic_types.hpp>
#include <jau/octets.hpp>

#include "BTTypes0.hpp"
#include "BTIoctl.hpp"
#include "HCIIoctl.hpp"

#include "SMPTypes.hpp"

/**
 * - - - - - - - - - - - - - - -
 *
 * HCITypes.hpp Module for HCIPacket Types, HCIStatusCode etc:
 *
 * - BT Core Spec v5.2: Vol 4, Part E Host Controller Interface (HCI): 7 HCI commands and events
 *
 */
namespace direct_bt {

    class HCIException : public jau::RuntimeException {
        protected:
            HCIException(std::string const type, std::string const m, const char* file, int line) noexcept
            : RuntimeException(type, m, file, line) {}

        public:
            HCIException(std::string const m, const char* file, int line) noexcept
            : RuntimeException("HCIException", m, file, line) {}
    };

    class HCIPacketException : public HCIException {
        public:
            HCIPacketException(std::string const m, const char* file, int line) noexcept
            : HCIException("HCIPacketException", m, file, line) {}
    };
    class HCIOpcodeException : public HCIException {
        public:
            HCIOpcodeException(std::string const m, const char* file, int line) noexcept
            : HCIException("HCIOpcodeException", m, file, line) {}
    };

    enum class HCIConstInt : int32_t {
        /** le connection supervisor timeout minimum of 500ms, see getHCIConnSupervisorTimeout() and v5.2 Vol 4, Part E - 7.8.12. */
        LE_CONN_MIN_TIMEOUT_MS  = 500
    };
    constexpr int32_t number(const HCIConstInt rhs) noexcept {
        return static_cast<int>(rhs);
    }

    /**
     * Defining the supervising timeout for LE connections to be a multiple of the maximum connection interval as follows:
     * <pre>
     *  ( 1 + conn_latency ) * conn_interval_max_ms * max(2, multiplier) [ms]
     * </pre>
     * If above result is smaller than the given min_result_ms, min_result_ms/10 will be returned.
     * @param conn_latency the connection latency
     * @param conn_interval_max_ms the maximum connection interval in [ms]
     * @param min_result_ms the minimum resulting supervisor timeout, defaults to HCIConstInt::LE_CONN_MIN_TIMEOUT_MS.
     *        If above formula results in a smaller value, min_result_ms/10 will be returned.
     * @param multiplier recommendation is 6, we use 10 as default for safety.
     * @return the resulting supervising timeout in 1/10 [ms], suitable for the HCIHandler::le_create_conn() command.
     */
    constexpr int32_t getHCIConnSupervisorTimeout(const uint16_t conn_latency, const uint16_t conn_interval_max_ms,
                                                  const uint16_t min_result_ms=number(HCIConstInt::LE_CONN_MIN_TIMEOUT_MS),
                                                  const uint16_t multiplier=10) noexcept
    {
        return std::max<uint16_t>(min_result_ms,
                                  ( 1 + conn_latency ) * conn_interval_max_ms * std::max<uint16_t>(2, multiplier)
                                 ) / 10;
    }

    /**
     * Supervisor timeout shall be in the range `[0 - ((supervision_timeout_ms / (conn_interval_max_ms*2)) - 1)]`.
     * @param supervision_timeout_ms
     * @param conn_interval_max_ms
     * @return maximum supervisor timeout, applicable to given parameter
     */
    constexpr int32_t getHCIMaxConnLatency(const int16_t supervision_timeout_ms, const int16_t conn_interval_max_ms) {
        return ((supervision_timeout_ms / (conn_interval_max_ms*2)) - 1);
    }

    enum class HCIConstU16 : uint16_t {
        INDEX_NONE             = 0xFFFF,
        /* Net length w/o null-termination */
        MAX_NAME_LENGTH        = 248,
        MAX_SHORT_NAME_LENGTH  =  10,
        MAX_AD_LENGTH          =  31
    };
    constexpr uint16_t number(const HCIConstU16 rhs) noexcept {
        return static_cast<uint16_t>(rhs);
    }

    /**
     * BT Core Spec v5.2: Vol 1, Part F Controller Error Codes: 1.3 List of Error Codes
     * <p>
     * BT Core Spec v5.2: Vol 1, Part F Controller Error Codes: 2 Error code descriptions
     * </p>
     */
    enum class HCIStatusCode : uint8_t {
        SUCCESS = 0x00,
        UNKNOWN_HCI_COMMAND = 0x01,
        UNKNOWN_CONNECTION_IDENTIFIER = 0x02,
        HARDWARE_FAILURE = 0x03,
        PAGE_TIMEOUT = 0x04,
        AUTHENTICATION_FAILURE = 0x05,
        PIN_OR_KEY_MISSING = 0x06,
        MEMORY_CAPACITY_EXCEEDED = 0x07,
        CONNECTION_TIMEOUT = 0x08,
        CONNECTION_LIMIT_EXCEEDED = 0x09,
        SYNC_DEVICE_CONNECTION_LIMIT_EXCEEDED = 0x0a,
        CONNECTION_ALREADY_EXISTS = 0x0b,
        COMMAND_DISALLOWED = 0x0c,
        CONNECTION_REJECTED_LIMITED_RESOURCES = 0x0d,
        CONNECTION_REJECTED_SECURITY = 0x0e,
        CONNECTION_REJECTED_UNACCEPTABLE_BD_ADDR = 0x0f,
        CONNECTION_ACCEPT_TIMEOUT_EXCEEDED = 0x10,
        UNSUPPORTED_FEATURE_OR_PARAM_VALUE = 0x11,
        INVALID_HCI_COMMAND_PARAMETERS = 0x12,
        REMOTE_USER_TERMINATED_CONNECTION = 0x13,
        REMOTE_DEVICE_TERMINATED_CONNECTION_LOW_RESOURCES = 0x14,
        REMOTE_DEVICE_TERMINATED_CONNECTION_POWER_OFF = 0x15,
        CONNECTION_TERMINATED_BY_LOCAL_HOST = 0x16,
        REPEATED_ATTEMPTS = 0x17,
        PAIRING_NOT_ALLOWED = 0x18,
        UNKNOWN_LMP_PDU = 0x19,
        UNSUPPORTED_REMOTE_OR_LMP_FEATURE = 0x1a,
        SCO_OFFSET_REJECTED = 0x1b,
        SCO_INTERVAL_REJECTED = 0x1c,
        SCO_AIR_MODE_REJECTED = 0x1d,
        INVALID_LMP_OR_LL_PARAMETERS = 0x1e,
        UNSPECIFIED_ERROR = 0x1f,
        UNSUPPORTED_LMP_OR_LL_PARAMETER_VALUE = 0x20,
        ROLE_CHANGE_NOT_ALLOWED = 0x21,
        LMP_OR_LL_RESPONSE_TIMEOUT = 0x22,
        LMP_OR_LL_COLLISION = 0x23,
        LMP_PDU_NOT_ALLOWED = 0x24,
        ENCRYPTION_MODE_NOT_ACCEPTED = 0x25,
        LINK_KEY_CANNOT_BE_CHANGED = 0x26,
        REQUESTED_QOS_NOT_SUPPORTED = 0x27,
        INSTANT_PASSED = 0x28,
        PAIRING_WITH_UNIT_KEY_NOT_SUPPORTED = 0x29,
        DIFFERENT_TRANSACTION_COLLISION = 0x2a,
        QOS_UNACCEPTABLE_PARAMETER = 0x2c,
        QOS_REJECTED = 0x2d,
        CHANNEL_ASSESSMENT_NOT_SUPPORTED = 0x2e,
        INSUFFICIENT_SECURITY = 0x2f,
        PARAMETER_OUT_OF_RANGE = 0x30,
        ROLE_SWITCH_PENDING = 0x32,
        RESERVED_SLOT_VIOLATION = 0x34,
        ROLE_SWITCH_FAILED = 0x35,
        EIR_TOO_LARGE = 0x36,
        SIMPLE_PAIRING_NOT_SUPPORTED_BY_HOST = 0x37,
        HOST_BUSY_PAIRING = 0x38,
        CONNECTION_REJECTED_NO_SUITABLE_CHANNEL = 0x39,
        CONTROLLER_BUSY = 0x3a,
        UNACCEPTABLE_CONNECTION_PARAM = 0x3b,
        ADVERTISING_TIMEOUT = 0x3c,
        CONNECTION_TERMINATED_MIC_FAILURE = 0x3d,
        CONNECTION_EST_FAILED_OR_SYNC_TIMEOUT = 0x3e,
        MAX_CONNECTION_FAILED  = 0x3f,
        COARSE_CLOCK_ADJ_REJECTED = 0x40,
        TYPE0_SUBMAP_NOT_DEFINED = 0x41,
        UNKNOWN_ADVERTISING_IDENTIFIER = 0x42,
        LIMIT_REACHED = 0x43,
        OPERATION_CANCELLED_BY_HOST = 0x44,
        PACKET_TOO_LONG = 0x45,

        // MgmtStatus -> HCIStatusCode

        FAILED              = 0xc3,
        CONNECT_FAILED      = 0xc4,
        AUTH_FAILED         = 0xc5,
        NOT_PAIRED          = 0xc6,
        NO_RESOURCES        = 0xc7,
        TIMEOUT             = 0xc8,
        ALREADY_CONNECTED   = 0xc9,
        BUSY                = 0xca,
        REJECTED            = 0xcb,
        NOT_SUPPORTED       = 0xcc,
        INVALID_PARAMS      = 0xcd,
        DISCONNECTED        = 0xce,
        NOT_POWERED         = 0xcf,
        CANCELLED           = 0xd0,
        INVALID_INDEX       = 0xd1,
        RFKILLED            = 0xd2,
        ALREADY_PAIRED      = 0xd3,
        PERMISSION_DENIED   = 0xd4,

        // Direct-BT

        INTERNAL_TIMEOUT = 0xfd,
        INTERNAL_FAILURE = 0xfe,
        UNKNOWN = 0xff
    };
    constexpr uint8_t number(const HCIStatusCode rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    std::string to_string(const HCIStatusCode ec) noexcept;

    class HCIStatusCodeCategory : public std::error_category {
        public:
            const char* name() const noexcept override { return "HCI"; }
            std::string message(int condition) const override {
                return "HCI::"+to_string( static_cast<HCIStatusCode>(condition) );
            }
            static HCIStatusCodeCategory& get() {
                static HCIStatusCodeCategory s;
                return s;
            }
    };
    inline std::error_code make_error_code( HCIStatusCode e ) noexcept {
      return std::error_code( number(e), HCIStatusCodeCategory::get() );
    }

    enum class HCIConstSizeT : jau::nsize_t {
        /** HCIPacketType::COMMAND header size including HCIPacketType */
        COMMAND_HDR_SIZE  = 1+3,
        /** HCIPacketType::ACLDATA header size including HCIPacketType */
        ACL_HDR_SIZE      = 1+4,
        /** HCIPacketType::SCODATA header size including HCIPacketType */
        SCO_HDR_SIZE      = 1+3,
        /** HCIPacketType::EVENT header size including HCIPacketType */
        EVENT_HDR_SIZE    = 1+2,
        /** Total packet size, guaranteed to be handled by adapter. */
        PACKET_MAX_SIZE   = 255
    };
    constexpr jau::nsize_t number(const HCIConstSizeT rhs) noexcept {
        return static_cast<jau::nsize_t>(rhs);
    }

    enum class HCIPacketType : uint8_t {
        COMMAND = 0x01,
        ACLDATA = 0x02,
        SCODATA = 0x03,
        EVENT   = 0x04,
        DIAG    = 0xf0,
        VENDOR  = 0xff
    };
    constexpr uint8_t number(const HCIPacketType rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    std::string to_string(const HCIPacketType op) noexcept;

    enum class HCIOGF : uint8_t {
        /** link control commands */
        LINK_CTL    = 0x01,
        /** link policy commands */
        LINK_POLICY = 0x02,
        /** controller baseband commands */
        BREDR_CTL   = 0x03,
        /** LE controller commands */
        LE_CTL      = 0x08
    };
    constexpr uint8_t number(const HCIOGF rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    std::string to_string(const HCIOGF op) noexcept;

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.7 Events
     */
    enum class HCIEventType : uint8_t {
        INVALID                         = 0x00,
        INQUIRY_COMPLETE                = 0x01,
        INQUIRY_RESULT                  = 0x02,
        CONN_COMPLETE                   = 0x03,
        CONN_REQUEST                    = 0x04,
        DISCONN_COMPLETE                = 0x05,
        AUTH_COMPLETE                   = 0x06,
        REMOTE_NAME                     = 0x07,
        ENCRYPT_CHANGE                  = 0x08,
        CHANGE_LINK_KEY_COMPLETE        = 0x09,
        REMOTE_FEATURES                 = 0x0b,
        REMOTE_VERSION                  = 0x0c,
        QOS_SETUP_COMPLETE              = 0x0d,
        CMD_COMPLETE                    = 0x0e,
        CMD_STATUS                      = 0x0f,
        HARDWARE_ERROR                  = 0x10,
        ROLE_CHANGE                     = 0x12,
        NUM_COMP_PKTS                   = 0x13,
        MODE_CHANGE                     = 0x14,
        PIN_CODE_REQ                    = 0x16,
        LINK_KEY_REQ                    = 0x17,
        LINK_KEY_NOTIFY                 = 0x18,
        CLOCK_OFFSET                    = 0x1c,
        PKT_TYPE_CHANGE                 = 0x1d,
        ENCRYPT_KEY_REFRESH_COMPLETE    = 0x30,
        IO_CAPABILITY_REQUEST           = 0x31,
        IO_CAPABILITY_RESPONSE          = 0x32,
        LE_META                         = 0x3e,
        DISCONN_PHY_LINK_COMPLETE       = 0x42,
        DISCONN_LOGICAL_LINK_COMPLETE   = 0x46,
        AMP_Receiver_Report             = 0x4b
        // etc etc - incomplete
    };
    constexpr uint8_t number(const HCIEventType rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    std::string to_string(const HCIEventType op) noexcept;

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.65 LE Meta event
     */
    enum class HCIMetaEventType : uint8_t {
        INVALID                             = 0x00,
        LE_CONN_COMPLETE                    = 0x01,/**< LE_CONN_COMPLETE */
        LE_ADVERTISING_REPORT               = 0x02,/**< LE_ADVERTISING_REPORT */
        LE_CONN_UPDATE_COMPLETE             = 0x03,/**< LE_CONN_UPDATE_COMPLETE */
        LE_REMOTE_FEAT_COMPLETE             = 0x04,/**< LE_REMOTE_FEAT_COMPLETE */
        LE_LTK_REQUEST                      = 0x05,/**< LE_LTK_REQUEST */
        LE_REMOTE_CONN_PARAM_REQ            = 0x06,/**< LE_REMOTE_CONN_PARAM_REQ */
        LE_DATA_LENGTH_CHANGE               = 0x07,/**< LE_DATA_LENGTH_CHANGE */
        LE_READ_LOCAL_P256_PUBKEY_COMPLETE  = 0x08,/**< LE_READ_LOCAL_P256_PUBKEY_COMPLETE */
        LE_GENERATE_DHKEY_COMPLETE          = 0x09,/**< LE_GENERATE_DHKEY_COMPLETE */
        LE_EXT_CONN_COMPLETE                = 0x0A,/**< LE_ENHANCED_CONN_COMPLETE */
        LE_DIRECT_ADV_REPORT                = 0x0B,/**< LE_DIRECT_ADV_REPORT */
        LE_PHY_UPDATE_COMPLETE              = 0x0C,/**< LE_PHY_UPDATE_COMPLETE */
        LE_EXT_ADV_REPORT                   = 0x0D,/**< LE_EXT_ADV_REPORT */
        LE_PERIODIC_ADV_SYNC_ESTABLISHED    = 0x0E,/**< LE_PERIODIC_ADV_SYNC_ESTABLISHED */
        LE_PERIODIC_ADV_REPORT              = 0x0F,/**< LE_PERIODIC_ADV_REPORT */
        LE_PERIODIC_ADV_SYNC_LOST           = 0x10,/**< LE_PERIODIC_ADV_SYNC_LOST */
        LE_SCAN_TIMEOUT                     = 0x11,/**< LE_SCAN_TIMEOUT */
        LE_ADV_SET_TERMINATED               = 0x12,/**< LE_ADV_SET_TERMINATED */
        LE_SCAN_REQ_RECEIVED                = 0x13,/**< LE_SCAN_REQ_RECEIVED */
        LE_CHANNEL_SEL_ALGO                 = 0x14,/**< LE_CHANNEL_SEL_ALGO */
        LE_CONNLESS_IQ_REPORT               = 0x15,/**< LE_CONNLESS_IQ_REPORT */
        LE_CONN_IQ_REPORT                   = 0x16,/**< LE_CONN_IQ_REPORT */
        LE_CTE_REQ_FAILED                   = 0x17,/**< LE_CTE_REQ_FAILED */
        LE_PERIODIC_ADV_SYNC_TRANSFER_RECV  = 0x18,/**< LE_PERIODIC_ADV_SYNC_TRANSFER_RECV */
        LE_CIS_ESTABLISHED                  = 0x19,/**< LE_CIS_ESTABLISHED */
        LE_CIS_REQUEST                      = 0x1A,/**< LE_CIS_REQUEST */
        LE_CREATE_BIG_COMPLETE              = 0x1B,/**< LE_CREATE_BIG_COMPLETE */
        LE_TERMINATE_BIG_COMPLETE           = 0x1C,/**< LE_TERMINATE_BIG_COMPLETE */
        LE_BIG_SYNC_ESTABLISHED             = 0x1D,/**< LE_BIG_SYNC_ESTABLISHED */
        LE_BIG_SYNC_LOST                    = 0x1E,/**< LE_BIG_SYNC_LOST */
        LE_REQUEST_PEER_SCA_COMPLETE        = 0x1F,/**< LE_REQUEST_PEER_SCA_COMPLETE */
        LE_PATH_LOSS_THRESHOLD              = 0x20,/**< LE_PATH_LOSS_THRESHOLD */
        LE_TRANSMIT_POWER_REPORTING         = 0x21,/**< LE_TRANSMIT_POWER_REPORTING */
        LE_BIGINFO_ADV_REPORT               = 0x22 /**< LE_BIGINFO_ADV_REPORT */
    };
    constexpr uint8_t number(const HCIMetaEventType rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }
    std::string to_string(const HCIMetaEventType op) noexcept;

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.1 Link Controller commands
     * <p>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.3 Controller & Baseband commands
     * </p>
     * <p>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.4 Informational paramters
     * </p>
     * <p>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8 LE Controller commands
     * </p>
     */
    enum class HCIOpcode : uint16_t {
        SPECIAL                     = 0x0000,/**< SPECIAL */
        CREATE_CONN                 = 0x0405,
        DISCONNECT                  = 0x0406,
        IO_CAPABILITY_REQ_REPLY     = 0x042b,
        IO_CAPABILITY_REQ_NEG_REPLY = 0x0434,
        SET_EVENT_MASK              = 0x0C01,/**< SET_EVENT_MASK */
        RESET                       = 0x0C03,
        READ_LOCAL_VERSION          = 0x1001,
        READ_LOCAL_COMMANDS         = 0x1002,
        LE_SET_EVENT_MASK           = 0x2001,/**< LE_SET_EVENT_MASK */
        LE_READ_BUFFER_SIZE         = 0x2002,
        LE_READ_LOCAL_FEATURES      = 0x2003,
        LE_SET_RANDOM_ADDR          = 0x2005,
        LE_SET_ADV_PARAM            = 0x2006,
        LE_READ_ADV_TX_POWER        = 0x2007,
        LE_SET_ADV_DATA             = 0x2008,
        LE_SET_SCAN_RSP_DATA        = 0x2009,
        LE_SET_ADV_ENABLE           = 0x200a,
        LE_SET_SCAN_PARAM           = 0x200b,
        LE_SET_SCAN_ENABLE          = 0x200c,
        LE_CREATE_CONN              = 0x200d, /**< LE_CREATE_CONN */
        LE_CREATE_CONN_CANCEL       = 0x200e,
        LE_READ_WHITE_LIST_SIZE     = 0x200f,
        LE_CLEAR_WHITE_LIST         = 0x2010,
        LE_ADD_TO_WHITE_LIST        = 0x2011,
        LE_DEL_FROM_WHITE_LIST      = 0x2012,
        LE_CONN_UPDATE              = 0x2013,
        LE_READ_REMOTE_FEATURES     = 0x2016,
        LE_ENABLE_ENC               = 0x2019,
        LE_LTK_REPLY_ACK            = 0x201A,
        LE_LTK_REPLY_REJ            = 0x201B,
        LE_READ_PHY                 = 0x2030,
        LE_SET_DEFAULT_PHY          = 0x2031,
        LE_SET_PHY                  = 0x2032,
        LE_SET_EXT_ADV_PARAMS       = 0x2036,
        LE_SET_EXT_ADV_DATA         = 0x2037,
        LE_SET_EXT_SCAN_RSP_DATA    = 0x2038,
        LE_SET_EXT_ADV_ENABLE       = 0x2039,
        LE_SET_EXT_SCAN_PARAMS      = 0x2041,
        LE_SET_EXT_SCAN_ENABLE      = 0x2042,
        LE_EXT_CREATE_CONN          = 0x2043,
        // etc etc - incomplete
    };
    constexpr uint16_t number(const HCIOpcode rhs) noexcept {
        return static_cast<uint16_t>(rhs);
    }
    std::string to_string(const HCIOpcode op) noexcept;

    enum class HCIOpcodeBit : uint8_t {
        SPECIAL                     =  0,
        CREATE_CONN                 =  3,
        DISCONNECT                  =  4,
        IO_CAPABILITY_REQ_REPLY     =  5,
        IO_CAPABILITY_REQ_NEG_REPLY =  6,
        SET_EVENT_MASK              =  7,
        RESET                       =  8,
        READ_LOCAL_VERSION          = 10,
        READ_LOCAL_COMMANDS         = 11,
        LE_SET_EVENT_MASK           = 20,
        LE_READ_BUFFER_SIZE         = 21,
        LE_READ_LOCAL_FEATURES      = 22,
        LE_SET_RANDOM_ADDR          = 23,
        LE_SET_ADV_PARAM            = 24,
        LE_READ_ADV_TX_POWER        = 25,
        LE_SET_ADV_DATA             = 26,
        LE_SET_SCAN_RSP_DATA        = 27,
        LE_SET_ADV_ENABLE           = 28,
        LE_SET_SCAN_PARAM           = 29,
        LE_SET_SCAN_ENABLE          = 30,
        LE_CREATE_CONN              = 31,
        LE_CREATE_CONN_CANCEL       = 32,
        LE_READ_WHITE_LIST_SIZE     = 33,
        LE_CLEAR_WHITE_LIST         = 34,
        LE_ADD_TO_WHITE_LIST        = 35,
        LE_DEL_FROM_WHITE_LIST      = 36,
        LE_CONN_UPDATE              = 37,
        LE_READ_REMOTE_FEATURES     = 38,
        LE_ENABLE_ENC               = 39,
        LE_LTK_REPLY_ACK            = 40,
        LE_LTK_REPLY_REJ            = 41,
        LE_READ_PHY                 = 46,
        LE_SET_DEFAULT_PHY          = 47,
        LE_SET_PHY                  = 48,
        LE_SET_EXT_ADV_PARAMS       = 52,
        LE_SET_EXT_ADV_DATA         = 53,
        LE_SET_EXT_SCAN_RSP_DATA    = 54,
        LE_SET_EXT_ADV_ENABLE       = 55,
        LE_SET_EXT_SCAN_PARAMS      = 56,
        LE_SET_EXT_SCAN_ENABLE      = 57,
        LE_EXT_CREATE_CONN          = 58
        // etc etc - incomplete
    };
    constexpr uint8_t number(const HCIOpcodeBit rhs) noexcept {
        return static_cast<uint8_t>(rhs);
    }

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 5.4 Exchange of HCI-specific information
     * <p>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 5.4.1 HCI Command packet
     * </p>
     * <p>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 5.4.4 HCI Event packet
     * </p>
     * <pre>
        __u8    packet_type;
     * </pre>
     */
    class HCIPacket
    {
        template<typename T> friend class HCIStructCmdCompleteEvtWrap;
        template<typename T> friend class HCIStructCmdCompleteMetaEvtWrap;

        protected:
            jau::POctets pdu;

            inline static void checkPacketType(const HCIPacketType type) {
                switch(type) {
                    case HCIPacketType::COMMAND:
                    case HCIPacketType::ACLDATA:
                    case HCIPacketType::SCODATA:
                    case HCIPacketType::EVENT:
                    case HCIPacketType::DIAG:
                    case HCIPacketType::VENDOR:
                        return; // OK
                    default:
                        throw HCIPacketException("Unsupported packet type "+jau::to_hexstring(number(type)), E_FILE_LINE);
                }
            }

            virtual std::string nameString() const noexcept { return "HCIPacket"; }

            virtual std::string baseString() const noexcept { return ""; }

            virtual std::string valueString() const noexcept { return ""; }

        public:
            HCIPacket(const HCIPacketType type, const jau::nsize_t total_packet_size)
            : pdu(total_packet_size, jau::endian::little)
            {
                if( 0 == total_packet_size ) {
                    throw jau::IndexOutOfBoundsException(1, total_packet_size, E_FILE_LINE);
                }
                pdu.put_uint8_nc(0, number(type));
            }

            /** Persistent memory, w/ ownership ..*/
            HCIPacket(const uint8_t *packet_data, const jau::nsize_t total_packet_size)
            : pdu(packet_data, total_packet_size, jau::endian::little)
            {
                if( 0 == total_packet_size ) {
                    throw jau::IndexOutOfBoundsException(1, total_packet_size, E_FILE_LINE);
                }
                checkPacketType(getPacketType());
            }

            virtual ~HCIPacket() noexcept {}

            /**
             * Clone template for convenience, based on derived class's copy-constructor.
             * @tparam T The derived definite class type, deducible by source argument
             * @param source the source to be copied
             * @return a new instance.
             */
            template<class T>
            static T* clone(const T& source) noexcept { return new T(source); }

            constexpr jau::nsize_t getTotalSize() const noexcept { return pdu.size(); }

            /** Return the underlying octets read only */
            jau::TROOctets & getPDU() noexcept { return pdu; }

            HCIPacketType getPacketType() noexcept { return static_cast<HCIPacketType>(pdu.get_uint8_nc(0)); }

            std::string toString() const noexcept {
                return nameString()+"["+baseString()+", "+valueString()+"]";
            }
    };
    inline std::string to_string(const HCIPacket& p) noexcept { return p.toString(); }

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 5.4.1 HCI Command packet
     * <p>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.8 LE Controller Commands
     * </p>
     * <pre>
        __le16  opcode; // OCF & OGF
        __u8    plen;
     * </pre>
     */
    class HCICommand : public HCIPacket
    {
        protected:
            inline static void checkOpcode(const HCIOpcode has, const HCIOpcode min, const HCIOpcode max)
            {
                if( has < min || has > max ) {
                    throw HCIOpcodeException("Has opcode "+jau::to_hexstring(number(has))+
                                     ", not within range ["+jau::to_hexstring(number(min))+
                                     ".."+jau::to_hexstring(number(max))+"]", E_FILE_LINE);
                }
            }

            std::string nameString() const noexcept override { return "HCICommand"; }

            std::string baseString() const noexcept override {
                return "opcode="+jau::to_hexstring(number(getOpcode()))+" "+to_string(getOpcode());
            }
            std::string valueString() const noexcept override {
                const jau::nsize_t psz = getParamSize();
                const std::string ps = psz > 0 ? jau::bytesHexString(getParam(), 0, psz, true /* lsbFirst */) : "";
                return "param[size "+std::to_string(getParamSize())+", data "+ps+"], tsz "+std::to_string(getTotalSize());
            }

        public:

            /** Enabling manual construction of command without given value. */
            HCICommand(const HCIOpcode opc, const jau::nsize_t param_size)
            : HCIPacket(HCIPacketType::COMMAND, number(HCIConstSizeT::COMMAND_HDR_SIZE)+param_size)
            {
                checkOpcode(opc, HCIOpcode::SPECIAL, HCIOpcode::LE_EXT_CREATE_CONN);
                if( 255 < param_size ) {
                    throw jau::IllegalArgumentException("HCICommand param size "+std::to_string(param_size)+" > 255", E_FILE_LINE);
                }

                pdu.put_uint16_nc(1, static_cast<uint16_t>(opc));
                pdu.put_uint8_nc(3, param_size);
            }

            /** Enabling manual construction of command with given value.  */
            HCICommand(const HCIOpcode opc, const uint8_t* param, const jau::nsize_t param_size)
            : HCICommand(opc, param_size)
            {
                if( param_size > 0 ) {
                    memcpy(pdu.get_wptr_nc(number(HCIConstSizeT::COMMAND_HDR_SIZE)), param, param_size);
                }
            }

            virtual ~HCICommand() noexcept {}

            HCIOpcode getOpcode() const noexcept { return static_cast<HCIOpcode>( pdu.get_uint16_nc(1) ); }
            jau::nsize_t getParamSize() const noexcept { return pdu.get_uint8_nc(3); }
            const uint8_t* getParam() const noexcept { return pdu.get_ptr_nc(number(HCIConstSizeT::COMMAND_HDR_SIZE)); }

            void trimParamSize(const jau::nsize_t param_size) {
                if( 255 < param_size ) {
                    throw jau::IllegalArgumentException("HCICommand new param size "+std::to_string(param_size)+" > 255", E_FILE_LINE);
                }
                if( getParamSize() < param_size ) {
                    throw jau::IllegalArgumentException("HCICommand new param size "+std::to_string(param_size)+" > old "+std::to_string(getParamSize()), E_FILE_LINE);
                }
                const jau::nsize_t new_total_packet_size = number(HCIConstSizeT::COMMAND_HDR_SIZE)+param_size;
                pdu.resize( new_total_packet_size );
                pdu.put_uint8_nc(3, param_size);
            }
    };

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.1.6 Disconnect command
     * <pre>
        Size 3
        __le16   handle;
        __u8     reason;
     * </pre>
     */
    class HCIDisconnectCmd : public HCICommand
    {
        protected:
            std::string nameString() const noexcept override { return "HCIDisconnectCmd"; }

        public:
            HCIDisconnectCmd(const uint16_t handle, HCIStatusCode reason)
            : HCICommand(HCIOpcode::DISCONNECT, 3)
            {
                pdu.put_uint16_nc(number(HCIConstSizeT::COMMAND_HDR_SIZE),handle);
                pdu.put_uint8_nc(number(HCIConstSizeT::COMMAND_HDR_SIZE)+2, number(reason));
            }
    };

    /**
     * Generic HCICommand wrapper for any HCI IOCTL structure
     * @tparam hcistruct the template typename, e.g. 'hci_cp_create_conn' for 'struct hci_cp_create_conn'
     */
    template<typename hcistruct>
    class HCIStructCommand : public HCICommand
    {
        protected:
            std::string nameString() const noexcept override { return "HCIStructCmd"; }

        public:
            /** Enabling manual construction of command with zero value. */
            HCIStructCommand(const HCIOpcode opc)
            : HCICommand(opc, sizeof(hcistruct))
            {
                hcistruct * cp = getWStruct();
                bzero((void*)cp, sizeof(hcistruct));
            }

            /** Enabling manual construction of command with given value.  */
            HCIStructCommand(const HCIOpcode opc, const hcistruct &cp)
            : HCICommand(opc, (const uint8_t *)(&cp), sizeof(hcistruct))
            { }

            const hcistruct * getStruct() const noexcept { return (const hcistruct *)(getParam()); }
            hcistruct * getWStruct() noexcept { return (hcistruct *)( pdu.get_wptr_nc( number(HCIConstSizeT::COMMAND_HDR_SIZE) ) ); }
    };

    class HCIHandler; // fwd

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 5.4.2 HCI ACL Data packets
     * <p>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.7 Events
     * </p>
     * <p>
     * ACL Data allows us to receive SMPPDUMsg inside an HCIACLData::l2cap_frame
     * via HCIACLData::getL2CAPFrame() and HCIACLData::l2cap_frame::getSMPPDUMsg().
     * </p>
     * <pre>
     *  uint16_t  handle;
     *  uint16_t  len;
     *  uint8_t   data[len];
     * </pre>
     */
    class HCIACLData : public HCIPacket
    {
        public:
            /**
             * Representing ACL Datas' L2CAP Frame
             * <p>
             * BT Core Spec v5.2: Vol 4, Part E HCI: 5.4.2 HCI ACL Data packets
             * </p>
             */
            struct l2cap_frame {
                /**
                 * The Packet_Boundary_Flag
                 * <p>
                 * BT Core Spec v5.2: Vol 4, Part E HCI: 5.4.2 HCI ACL Data packets
                 * </p>
                 */
                enum class PBFlag : uint8_t {
                    /** 0b00: Start of a non-automatically-flushable PDU from Host to Controller. Value 0b00. */
                    START_NON_AUTOFLUSH_HOST      =  0b00,
                    /** 0b01: Continuing fragment. Value 0b01. */
                    CONTINUING_FRAGMENT           =  0b01,
                    /** 0b10: Start of an automatically flushable PDU. Value 0b10. */
                    START_AUTOFLUSH               =  0b10,
                    /** A complete L2CAP PDU. Automatically flushable. Value 0b11.*/
                    COMPLETE_L2CAP_AUTOFLUSH      =  0b11,
                };
                static constexpr uint8_t number(const PBFlag v) noexcept { return static_cast<uint8_t>(v); }
                static std::string toString(const PBFlag v) noexcept;

                /** The connection handle */
                const uint16_t handle;
                const PBFlag pb_flag;
                /** The Broadcast_Flag */
                const uint8_t bc_flag;
                const L2CAP_CID cid;
                const L2CAP_PSM psm;
                const uint16_t len;

                constexpr bool isSMP() const noexcept { return L2CAP_CID::SMP == cid || L2CAP_CID::SMP_BREDR == cid; }

                constexpr bool isGATT() const noexcept { return L2CAP_CID::ATT == cid; }

                std::string toString() const noexcept {
                    return "l2cap[handle "+jau::to_hexstring(handle)+", flags[pb "+toString(pb_flag)+", bc "+jau::to_hexstring(bc_flag)+
                            "], cid "+to_string(cid)+
                            ", psm "+to_string(psm)+", len "+std::to_string(len)+ "]";
                }
                std::string toString(const uint8_t* l2cap_data) const noexcept {
                    const std::string ds = nullptr != l2cap_data && 0 < len ?  jau::bytesHexString(l2cap_data, 0, len, true /* lsbFirst*/) : "empty";
                    return "l2cap[handle "+jau::to_hexstring(handle)+", flags[pb "+toString(pb_flag)+", bc "+jau::to_hexstring(bc_flag)+
                            "], cid "+to_string(cid)+
                            ", psm "+to_string(psm)+", len "+std::to_string(len)+", data "+ds+"]";
                }
            };

            /**
             * Return a newly created specialized instance pointer to base class.
             * <p>
             * Returned memory reference is managed by caller (delete etc)
             * </p>
             */
            static std::unique_ptr<HCIACLData> getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept;

            /**
             * Returns the handle.
             */
            constexpr static uint16_t get_handle(const uint16_t handle_and_flags) noexcept { return static_cast<uint16_t>(handle_and_flags & 0x0fff); }

            /**
             * Returns the Packet_Boundary_Flag.
             */
            constexpr static uint8_t get_pbflag(const uint16_t handle_and_flags) noexcept { return static_cast<uint8_t>( ( handle_and_flags >> 12 ) & 0b11 ); }

            /**
             * Returns the Broadcast_Flag.
             */
            constexpr static uint8_t get_bcflag(const uint16_t handle_and_flags) noexcept { return static_cast<uint8_t>( ( handle_and_flags >> 14 ) & 0b11 ); }

            /** Persistent memory, w/ ownership ..*/
            HCIACLData(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : HCIPacket(buffer, buffer_len)
            {
                const jau::nsize_t baseParamSize = getParamSize();
                pdu.check_range(0, number(HCIConstSizeT::ACL_HDR_SIZE)+baseParamSize);
            }

            uint16_t getHandleAndFlags() const noexcept { return pdu.get_uint16_nc(1); }
            jau::nsize_t getParamSize() const noexcept { return pdu.get_uint16_nc(3); }
            const uint8_t* getParam() const noexcept { return pdu.get_ptr_nc(number(HCIConstSizeT::ACL_HDR_SIZE)); }

            l2cap_frame getL2CAPFrame(const uint8_t* & l2cap_data) const noexcept;

            std::string toString() const noexcept {
                const uint8_t* l2cap_data;
                return "ACLData[size "+std::to_string(getParamSize())+", data "+getL2CAPFrame(l2cap_data).toString(l2cap_data)+", tsz "+std::to_string(getTotalSize())+"]";
            }
            std::string toString(const l2cap_frame& l2cap, const uint8_t* l2cap_data) const noexcept {
                return "ACLData[size "+std::to_string(getParamSize())+", data "+l2cap.toString(l2cap_data)+", tsz "+std::to_string(getTotalSize())+"]";
            }
    };

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 5.4.4 HCI Event packet
     * <p>
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.7 Events
     * </p>
     * <pre>
        __u8    evt;
        __u8    plen;
     * </pre>
     */
    class HCIEvent : public HCIPacket
    {
        protected:
            uint64_t ts_creation;

            inline static void checkEventType(const HCIEventType has, const HCIEventType min, const HCIEventType max)
            {
                if( has < min || has > max ) {
                    throw HCIOpcodeException("Has evcode "+jau::to_hexstring(number(has))+
                                     ", not within range ["+jau::to_hexstring(number(min))+
                                     ".."+jau::to_hexstring(number(max))+"]", E_FILE_LINE);
                }
            }
            inline static void checkEventType(const HCIEventType has, const HCIEventType exp)
            {
                if( has != exp ) {
                    throw HCIOpcodeException("Has evcode "+jau::to_hexstring(number(has))+
                                     ", not matching "+jau::to_hexstring(number(exp)), E_FILE_LINE);
                }
            }

            std::string nameString() const noexcept override { return "HCIEvent"; }

            std::string baseString() const noexcept override {
                return "event="+jau::to_hexstring(number(getEventType()))+" "+to_string(getEventType());
            }
            std::string valueString() const noexcept override {
                const jau::nsize_t d_sz_base = getBaseParamSize();
                const jau::nsize_t d_sz = getParamSize();
                const std::string d_str = d_sz > 0 ? jau::bytesHexString(getParam(), 0, d_sz, true /* lsbFirst */) : "";
                return "data[size "+std::to_string(d_sz)+"/"+std::to_string(d_sz_base)+", data "+d_str+"], tsz "+std::to_string(getTotalSize());
            }

            jau::nsize_t getBaseParamSize() const noexcept { return pdu.get_uint8_nc(2); }

        public:

            /**
             * Return a newly created specialized instance pointer to base class.
             * <p>
             * Returned memory reference is managed by caller (delete etc)
             * </p>
             */
            static std::unique_ptr<HCIEvent> getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept;

            /** Persistent memory, w/ ownership ..*/
            HCIEvent(const uint8_t* buffer, const jau::nsize_t buffer_len, const jau::nsize_t exp_param_size)
            : HCIPacket(buffer, buffer_len), ts_creation(jau::getCurrentMilliseconds())
            {
                const jau::nsize_t baseParamSize = getBaseParamSize();
                pdu.check_range(0, number(HCIConstSizeT::EVENT_HDR_SIZE)+baseParamSize);
                if( exp_param_size > baseParamSize ) {
                    throw jau::IndexOutOfBoundsException(exp_param_size, baseParamSize, E_FILE_LINE);
                }
                checkEventType(getEventType(), HCIEventType::INQUIRY_COMPLETE, HCIEventType::AMP_Receiver_Report);
            }

            /** Enabling manual construction of event without given value.  */
            HCIEvent(const HCIEventType evt, const jau::nsize_t param_size=0)
            : HCIPacket(HCIPacketType::EVENT, number(HCIConstSizeT::EVENT_HDR_SIZE)+param_size), ts_creation(jau::getCurrentMilliseconds())
            {
                checkEventType(evt, HCIEventType::INQUIRY_COMPLETE, HCIEventType::AMP_Receiver_Report);
                pdu.put_uint8_nc(1, number(evt));
                pdu.put_uint8_nc(2, param_size);
            }

            /** Enabling manual construction of event with given value.  */
            HCIEvent(const HCIEventType evt, const uint8_t* param, const jau::nsize_t param_size)
            : HCIEvent(evt, param_size)
            {
                if( param_size > 0 ) {
                    memcpy(pdu.get_wptr_nc(number(HCIConstSizeT::EVENT_HDR_SIZE)), param, param_size);
                }
            }

            virtual ~HCIEvent() noexcept {}

            uint64_t getTimestamp() const noexcept { return ts_creation; }

            constexpr HCIEventType getEventType() const noexcept { return static_cast<HCIEventType>( pdu.get_uint8_nc(1) ); }
            constexpr bool isEvent(HCIEventType t) const noexcept { return t == getEventType(); }

            /**
             * The meta subevent type
             */
            virtual HCIMetaEventType getMetaEventType() const noexcept { return HCIMetaEventType::INVALID; }
            bool isMetaEvent(HCIMetaEventType t) const noexcept { return t == getMetaEventType(); }

            virtual jau::nsize_t getParamSize() const noexcept { return getBaseParamSize(); }
            virtual const uint8_t* getParam() const noexcept { return pdu.get_ptr_nc(number(HCIConstSizeT::EVENT_HDR_SIZE)); }

            virtual bool validate(const HCICommand & cmd) const noexcept { (void)cmd; return true; }
    };

    /**
     * Generic HCIEvent wrapper for any HCI IOCTL 'command complete' alike event struct having a HCIStatusCode uint8_t status field.
     * @tparam hcistruct the template typename, e.g. 'hci_ev_conn_complete' for 'struct hci_ev_conn_complete'
     */
    template<typename hcistruct>
    class HCIStructCmdCompleteEvtWrap
    {
        private:
            HCIEvent &orig;

        public:
            HCIStructCmdCompleteEvtWrap(HCIEvent & orig_)
            : orig(orig_)
            { }
            std::string toString() const noexcept { return orig.toString(); }

            bool isTypeAndSizeValid(const HCIEventType ec) const noexcept {
                return orig.isEvent(ec) &&
                       orig.pdu.is_range_valid(0, number(HCIConstSizeT::EVENT_HDR_SIZE)+sizeof(hcistruct));
            }
            const hcistruct * getStruct() const noexcept { return (const hcistruct *)( orig.getParam() ); }
            HCIStatusCode getStatus() const noexcept { return static_cast<HCIStatusCode>( getStruct()->status ); }

            hcistruct * getWStruct() noexcept { return (hcistruct *)( orig.pdu.get_wptr_nc(number(HCIConstSizeT::EVENT_HDR_SIZE)) ); }
    };


    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.5 Disconnection Complete event
     * <p>
     * Size 4
        __u8     status;
        __le16   handle;
        __u8     reason;
     * </p>
     */
    class HCIDisconnectionCompleteEvent : public HCIEvent
    {
        protected:
            std::string nameString() const noexcept override { return "HCIDisconnectionCompleteEvent"; }

            std::string baseString() const noexcept override {
                return HCIEvent::baseString()+
                        ", status "+jau::to_hexstring(static_cast<uint8_t>(getStatus()))+" "+to_string(getStatus())+
                        ", handle "+jau::to_hexstring(getHandle())+
                        ", reason "+jau::to_hexstring(static_cast<uint8_t>(getReason()))+" "+to_string(getReason());
            }

        public:
            HCIDisconnectionCompleteEvent(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : HCIEvent(buffer, buffer_len, 4)
            {
                checkEventType(getEventType(), HCIEventType::DISCONN_COMPLETE);
            }

            HCIStatusCode getStatus() const noexcept { return static_cast<HCIStatusCode>( pdu.get_uint8_nc(number(HCIConstSizeT::EVENT_HDR_SIZE)) ); }
            uint16_t getHandle() const noexcept { return pdu.get_uint16_nc(number(HCIConstSizeT::EVENT_HDR_SIZE)+1); }
            HCIStatusCode getReason() const noexcept { return static_cast<HCIStatusCode>( pdu.get_uint8_nc(number(HCIConstSizeT::EVENT_HDR_SIZE)+3) ); }

            bool validate(const HCICommand & cmd) const noexcept override {
                return cmd.getOpcode() == HCIOpcode::DISCONNECT;
            }
    };

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.14 Command Complete event
     * <p>
     * Size 3 + return size
        __u8     ncmd;
        __le16   opcode;
        Return_Paramters of variable length, usually with '__u8 status' first.
     * </p>
     */
    class HCICommandCompleteEvent : public HCIEvent
    {
        protected:
            std::string nameString() const noexcept override { return "HCICmdCompleteEvent"; }

            std::string baseString() const noexcept override {
                return HCIEvent::baseString()+", opcode="+jau::to_hexstring(static_cast<uint16_t>(getOpcode()))+
                        " "+to_string(getOpcode())+
                        ", ncmd "+std::to_string(getNumCommandPackets());
            }

        public:
            HCICommandCompleteEvent(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : HCIEvent(buffer, buffer_len, 3)
            {
                checkEventType(getEventType(), HCIEventType::CMD_COMPLETE);
            }

            /**
             * The Number of HCI Command packets which are allowed to be sent to the Controller from the Host.
             * <p>
             * Range: 0 to 255
             * </p>
             */
            uint8_t getNumCommandPackets() const noexcept { return pdu.get_uint8_nc(number(HCIConstSizeT::EVENT_HDR_SIZE)+0); }

            /**
             * The associated command
             */
            HCIOpcode getOpcode() const noexcept { return static_cast<HCIOpcode>( pdu.get_uint16_nc(number(HCIConstSizeT::EVENT_HDR_SIZE)+1) ); }

            jau::nsize_t getReturnParamSize() const noexcept { return getParamSize() - 3; }
            const uint8_t* getReturnParam() const { return pdu.get_ptr(number(HCIConstSizeT::EVENT_HDR_SIZE)+3); }

            bool validate(const HCICommand & cmd) const noexcept override {
                return cmd.getOpcode() == getOpcode();
            }
    };

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.15 Command Status event
     * <p>
     * Size 4
        __u8     status;
        __u8     ncmd;
        __le16   opcode;
     * </p>
     */
    class HCICommandStatusEvent : public HCIEvent
    {
        protected:
            std::string nameString() const noexcept override { return "HCICmdStatusEvent"; }

            std::string baseString() const noexcept override {
                return HCIEvent::baseString()+", opcode="+jau::to_hexstring(static_cast<uint16_t>(getOpcode()))+
                        " "+to_string(getOpcode())+
                        ", ncmd "+std::to_string(getNumCommandPackets())+
                        ", status "+jau::to_hexstring(static_cast<uint8_t>(getStatus()))+" "+to_string(getStatus());
            }

        public:
            HCICommandStatusEvent(const uint8_t* buffer, const jau::nsize_t buffer_len)
            : HCIEvent(buffer, buffer_len, 4)
            {
                checkEventType(getEventType(), HCIEventType::CMD_STATUS);
            }

            HCIStatusCode getStatus() const noexcept { return static_cast<HCIStatusCode>( pdu.get_uint8_nc(number(HCIConstSizeT::EVENT_HDR_SIZE)) ); }

            /**
             * The Number of HCI Command packets which are allowed to be sent to the Controller from the Host.
             * <p>
             * Range: 0 to 255
             * </p>
             */
            uint8_t getNumCommandPackets() const noexcept { return pdu.get_uint8_nc(number(HCIConstSizeT::EVENT_HDR_SIZE)+1); }

            /**
             * The associated command
             */
            HCIOpcode getOpcode() const noexcept { return static_cast<HCIOpcode>( pdu.get_uint16_nc(number(HCIConstSizeT::EVENT_HDR_SIZE)+1+1) ); }

            bool validate(const HCICommand & cmd) const noexcept override {
                return cmd.getOpcode() == getOpcode();
            }
    };

    /**
     * BT Core Spec v5.2: Vol 4, Part E HCI: 7.7.65 LE Meta event
     * <p>
     * Size 1
        __u8     subevent;
     * </p>
     */
    class HCIMetaEvent : public HCIEvent
    {
        protected:
            std::string nameString() const noexcept override { return "HCIMetaEvent"; }

            static void checkMetaType(const HCIMetaEventType has, const HCIMetaEventType exp)
            {
                if( has != exp ) {
                    throw HCIOpcodeException("Has meta "+jau::to_hexstring(number(has))+
                                     ", not matching "+jau::to_hexstring(number(exp)), E_FILE_LINE);
                }
            }

            std::string baseString() const noexcept override {
                return "event="+jau::to_hexstring(number(getMetaEventType()))+" "+to_string(getMetaEventType())+" (le-meta)";
            }

        public:
            /** Passing through preset buffer of this type */
            HCIMetaEvent(const uint8_t* buffer, const jau::nsize_t buffer_len, const jau::nsize_t exp_meta_param_size)
            : HCIEvent(buffer, buffer_len, 1+exp_meta_param_size)
            {
                checkEventType(getEventType(), HCIEventType::LE_META);
            }

            /** Enabling manual construction of event without given value. */
            HCIMetaEvent(const HCIMetaEventType mc, const jau::nsize_t meta_param_size)
            : HCIEvent(HCIEventType::LE_META, 1+meta_param_size)
            {
                pdu.put_uint8_nc(number(HCIConstSizeT::EVENT_HDR_SIZE), number(mc));
            }

            /** Enabling manual construction of event with given value.  */
            HCIMetaEvent(const HCIMetaEventType mc, const uint8_t * meta_param, const jau::nsize_t meta_param_size)
            : HCIMetaEvent(mc, meta_param_size)
            {
                if( meta_param_size > 0 ) {
                    memcpy(pdu.get_wptr_nc(number(HCIConstSizeT::EVENT_HDR_SIZE)+1), meta_param, meta_param_size);
                }
            }

            HCIMetaEventType getMetaEventType() const noexcept override { return static_cast<HCIMetaEventType>( pdu.get_uint8_nc(number(HCIConstSizeT::EVENT_HDR_SIZE)) ); }

            jau::nsize_t getParamSize() const noexcept override { return HCIEvent::getParamSize()-1; }
            const uint8_t* getParam() const noexcept override { return HCIEvent::getParam()+1; }
    };

    /**
     * Generic HCIMetaEvent wrapper for any HCI IOCTL 'command complete' alike meta event struct having a HCIStatusCode uint8_t status field.
     * @tparam hcistruct the template typename, e.g. 'hci_ev_le_conn_complete' for 'struct hci_ev_le_conn_complete'
     */
    template<typename hcistruct>
    class HCIStructCmdCompleteMetaEvtWrap
    {
        private:
            HCIMetaEvent & orig;

        public:
            HCIStructCmdCompleteMetaEvtWrap(HCIMetaEvent & orig_)
            : orig(orig_)
            { }
            std::string toString() const noexcept { return orig.toString(); }

            bool isTypeAndSizeValid(const HCIMetaEventType mc) const noexcept {
                return orig.isMetaEvent(mc) &&
                       orig.pdu.is_range_valid(0, number(HCIConstSizeT::EVENT_HDR_SIZE)+1+sizeof(hcistruct));
            }
            const hcistruct * getStruct() const noexcept { return (const hcistruct *)( orig.getParam() ); }
            HCIStatusCode getStatus() const noexcept { return static_cast<HCIStatusCode>( getStruct()->status ); }

            // hcistruct * getWStruct() noexcept { return (hcistruct *)( orig.pdu.get_wptr_nc(number(HCIConstU8::EVENT_HDR_SIZE)+1) ); }
    };

    struct HCILocalVersion {
        uint8_t     hci_ver;
        uint16_t    hci_rev;
        uint8_t     lmp_ver;
        uint16_t    manufacturer;
        uint16_t    lmp_subver;

        std::string toString() noexcept;
    };

} // namespace direct_bt

namespace std
{
    template <>
        struct is_error_code_enum<direct_bt::HCIStatusCode> : true_type {};
}

#endif /* HCI_TYPES_HPP_ */
