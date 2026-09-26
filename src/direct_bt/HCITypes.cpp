/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020-2026 Gothel Software e.K.
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

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <cstdio>

#include <jau/ringbuffer.hpp>

#include "HCITypes.hpp"
#include "HCIHandler.hpp"

extern "C" {
    #include <unistd.h>
    #include <sys/param.h>
    #include <sys/uio.h>
    #include <sys/types.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <poll.h>
}

namespace direct_bt {

JAU_MAKE_ENUM_STRING_CODE(HCIStatusCode,
    SUCCESS, UNKNOWN_COMMAND, UNKNOWN_CONNECTION_IDENTIFIER, HARDWARE_FAILURE, PAGE_TIMEOUT, AUTHENTICATION_FAILURE, PIN_OR_KEY_MISSING,
    MEMORY_CAPACITY_EXCEEDED, CONNECTION_TIMEOUT, CONNECTION_LIMIT_EXCEEDED, SYNC_DEVICE_CONNECTION_LIMIT_EXCEEDED,
    CONNECTION_ALREADY_EXISTS, COMMAND_DISALLOWED,
    CONNECTION_REJECTED_LIMITED_RESOURCES, CONNECTION_REJECTED_SECURITY, CONNECTION_REJECTED_UNACCEPTABLE_BD_ADDR, CONNECTION_ACCEPT_TIMEOUT_EXCEEDED,
    UNSUPPORTED_FEATURE_OR_PARAM_VALUE, INVALID_HCI_COMMAND_PARAMETERS,
    REMOTE_USER_TERMINATED_CONNECTION, REMOTE_DEVICE_TERMINATED_CONNECTION_LOW_RESOURCES, REMOTE_DEVICE_TERMINATED_CONNECTION_POWER_OFF,
    CONNECTION_TERMINATED_BY_LOCAL_HOST, REPEATED_ATTEMPTS, PAIRING_NOT_ALLOWED, UNKNOWN_LMP_PDU, UNSUPPORTED_REMOTE_OR_LMP_FEATURE,
    SCO_OFFSET_REJECTED, SCO_INTERVAL_REJECTED, SCO_AIR_MODE_REJECTED, INVALID_LMP_OR_LL_PARAMETERS, UNSPECIFIED_ERROR,
    UNSUPPORTED_LMP_OR_LL_PARAMETER_VALUE, ROLE_CHANGE_NOT_ALLOWED, LMP_OR_LL_RESPONSE_TIMEOUT, LMP_OR_LL_COLLISION, LMP_PDU_NOT_ALLOWED,
    ENCRYPTION_MODE_NOT_ACCEPTED, LINK_KEY_CANNOT_BE_CHANGED, REQUESTED_QOS_NOT_SUPPORTED, INSTANT_PASSED, PAIRING_WITH_UNIT_KEY_NOT_SUPPORTED,
    DIFFERENT_TRANSACTION_COLLISION, QOS_UNACCEPTABLE_PARAMETER, QOS_REJECTED, CHANNEL_ASSESSMENT_NOT_SUPPORTED, INSUFFICIENT_SECURITY,
    PARAMETER_OUT_OF_RANGE, ROLE_SWITCH_PENDING, RESERVED_SLOT_VIOLATION, ROLE_SWITCH_FAILED, EIR_TOO_LARGE, SIMPLE_PAIRING_NOT_SUPPORTED_BY_HOST,
    HOST_BUSY_PAIRING, CONNECTION_REJECTED_NO_SUITABLE_CHANNEL, CONTROLLER_BUSY, UNACCEPTABLE_CONNECTION_PARAM, ADVERTISING_TIMEOUT,
    CONNECTION_TERMINATED_MIC_FAILURE, CONNECTION_EST_FAILED_OR_SYNC_TIMEOUT, MAX_CONNECTION_FAILED, COARSE_CLOCK_ADJ_REJECTED, TYPE0_SUBMAP_NOT_DEFINED,
    UNKNOWN_ADVERTISING_IDENTIFIER, LIMIT_REACHED, OPERATION_CANCELLED_BY_HOST, PACKET_TOO_LONG, FAILED, CONNECT_FAILED, AUTH_FAILED, NOT_PAIRED,
    NO_RESOURCES, TIMEOUT, ALREADY_CONNECTED, BUSY, REJECTED, NOT_SUPPORTED, INVALID_PARAMS, DISCONNECTED, NOT_POWERED, CANCELLED, INVALID_INDEX, RFKILLED,
    ALREADY_PAIRED, PERMISSION_DENIED, L2CAP_CLIENT_TIMEOUT, INTERNAL_TIMEOUT, INTERNAL_FAILURE, UNKNOWN);

JAU_MAKE_ENUM_STRING_CODE(HCIEventType,
    INVALID, INQUIRY_COMPLETE, INQUIRY_RESULT, CONN_COMPLETE, CONN_REQUEST, DISCONN_COMPLETE, AUTH_COMPLETE, REMOTE_NAME, ENCRYPT_CHANGE,
    CHANGE_LINK_KEY_COMPLETE, REMOTE_FEATURES, REMOTE_VERSION, QOS_SETUP_COMPLETE, CMD_COMPLETE, CMD_STATUS, HARDWARE_ERROR, ROLE_CHANGE,
    NUM_COMP_PKTS, MODE_CHANGE, PIN_CODE_REQ, LINK_KEY_REQ, LINK_KEY_NOTIFY, CLOCK_OFFSET, PKT_TYPE_CHANGE, ENCRYPT_KEY_REFRESH_COMPLETE,
    IO_CAPABILITY_REQUEST, IO_CAPABILITY_RESPONSE, LE_META, DISCONN_PHY_LINK_COMPLETE, DISCONN_LOGICAL_LINK_COMPLETE, AMP_Receiver_Report);

JAU_MAKE_ENUM_STRING_CODE(HCIMetaEventType,
    INVALID, LE_CONN_COMPLETE, LE_ADVERTISING_REPORT, LE_CONN_UPDATE_COMPLETE, LE_REMOTE_FEAT_COMPLETE, LE_LTK_REQUEST, LE_REMOTE_CONN_PARAM_REQ,
    LE_DATA_LENGTH_CHANGE, LE_READ_LOCAL_P256_PUBKEY_COMPLETE, LE_GENERATE_DHKEY_COMPLETE, LE_EXT_CONN_COMPLETE, LE_DIRECT_ADV_REPORT,
    LE_PHY_UPDATE_COMPLETE, LE_EXT_ADV_REPORT, LE_PERIODIC_ADV_SYNC_ESTABLISHED, LE_PERIODIC_ADV_REPORT, LE_PERIODIC_ADV_SYNC_LOST,
    LE_SCAN_TIMEOUT, LE_ADV_SET_TERMINATED, LE_SCAN_REQ_RECEIVED, LE_CHANNEL_SEL_ALGO, LE_CONNLESS_IQ_REPORT, LE_CONN_IQ_REPORT, LE_CTE_REQ_FAILED,
    LE_PERIODIC_ADV_SYNC_TRANSFER_RECV, LE_CIS_ESTABLISHED, LE_CIS_REQUEST, LE_CREATE_BIG_COMPLETE, LE_TERMINATE_BIG_COMPLETE, LE_BIG_SYNC_ESTABLISHED,
    LE_BIG_SYNC_LOST, LE_REQUEST_PEER_SCA_COMPLETE, LE_PATH_LOSS_THRESHOLD, LE_TRANSMIT_POWER_REPORTING, LE_BIGINFO_ADV_REPORT);

JAU_MAKE_ENUM_STRING_CODE(HCIOpcode,
    SPECIAL, CREATE_CONN, DISCONNECT, IO_CAPABILITY_REQ_REPLY, IO_CAPABILITY_REQ_NEG_REPLY, SET_EVENT_MASK, RESET, READ_LOCAL_VERSION,
    READ_LOCAL_COMMANDS, LE_SET_EVENT_MASK, LE_READ_BUFFER_SIZE, LE_READ_LOCAL_FEATURES, LE_SET_RANDOM_ADDR, LE_SET_ADV_PARAM, LE_READ_ADV_TX_POWER,
    LE_SET_ADV_DATA, LE_SET_SCAN_RSP_DATA, LE_SET_ADV_ENABLE, LE_SET_SCAN_PARAM, LE_SET_SCAN_ENABLE, LE_CREATE_CONN, LE_CREATE_CONN_CANCEL,
    LE_READ_WHITE_LIST_SIZE, LE_CLEAR_WHITE_LIST, LE_ADD_TO_WHITE_LIST, LE_DEL_FROM_WHITE_LIST, LE_CONN_UPDATE, LE_READ_REMOTE_FEATURES, LE_ENABLE_ENC,
    LE_LTK_REPLY_ACK, LE_LTK_REPLY_REJ, LE_ADD_TO_RESOLV_LIST, LE_DEL_FROM_RESOLV_LIST, LE_CLEAR_RESOLV_LIST, LE_READ_RESOLV_LIST_SIZE,
    LE_READ_PEER_RESOLV_ADDR, LE_READ_LOCAL_RESOLV_ADDR, LE_SET_ADDR_RESOLV_ENABLE, LE_READ_PHY, LE_SET_DEFAULT_PHY, LE_SET_PHY, LE_SET_EXT_ADV_PARAMS,
    LE_SET_EXT_ADV_DATA, LE_SET_EXT_SCAN_RSP_DATA, LE_SET_EXT_ADV_ENABLE, LE_SET_EXT_SCAN_PARAMS, LE_SET_EXT_SCAN_ENABLE, LE_EXT_CREATE_CONN);

JAU_MAKE_ENUM_STRING2_CODE(L2CapFrame::PBFlag, PBFlag,
    START_NON_AUTOFLUSH_HOST, CONTINUING_FRAGMENT, START_AUTOFLUSH, COMPLETE_L2CAP_AUTOFLUSH);

std::string to_string(const HCIPacketType op) noexcept {
    switch(op) {
        case HCIPacketType::COMMAND: return "COMMAND";
        case HCIPacketType::ACLDATA: return "ACLDATA";
        case HCIPacketType::SCODATA: return "SCODATA";
        case HCIPacketType::EVENT: return "EVENT";
        case HCIPacketType::DIAG: return "DIAG";
        case HCIPacketType::VENDOR: return "VENDOR";
    }
    return "Unknown HCIPacketType";
}

std::string to_string(const HCIOGF op) noexcept {
    (void)op;
    return "";
}

std::unique_ptr<HCICommand> HCICommand::getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept {
    const HCIPacketType pc = static_cast<HCIPacketType>( jau::get_uint8(buffer + 0) );

    if( HCIPacketType::COMMAND != pc ) {
        return nullptr;
    }

    const jau::nsize_t paramSize = buffer_size >= number(HCIConstSizeT::COMMAND_HDR_SIZE) ? jau::get_uint8(buffer + 3) : 0;
    if( buffer_size < number(HCIConstSizeT::COMMAND_HDR_SIZE) + paramSize ) {
        jau_WARN_PRINT("HCIEvent::getSpecialized: length mismatch %zu < COMMAND_HDR_SIZE(%zu) + %zu",
                buffer_size, *(HCIConstSizeT::COMMAND_HDR_SIZE), paramSize);
        return nullptr;
    }

    try {
        const HCIOpcode oc = static_cast<HCIOpcode>( jau::get_uint16(buffer + 1, jau::lb_endian_t::little) );
        switch( oc ) {
            case HCIOpcode::DISCONNECT:
                return std::make_unique<HCIDisconnectCmd>(buffer, buffer_size);
            case HCIOpcode::LE_ENABLE_ENC:
                return std::make_unique<HCILEEnableEncryptionCmd>(buffer, buffer_size);
            case HCIOpcode::LE_LTK_REPLY_ACK:
                return std::make_unique<HCILELTKReplyAckCmd>(buffer, buffer_size);
            case HCIOpcode::LE_LTK_REPLY_REJ:
                return std::make_unique<HCILELTKReplyRejCmd>(buffer, buffer_size);
            default:
                // No further specialization, use HCIStructCmdCompleteEvt template
                return std::make_unique<HCICommand>(buffer, buffer_size, 0);
        }
    } catch (...) {
        jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
    }
    return nullptr;
}

std::unique_ptr<HCIEvent> HCIEvent::getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept {
    const HCIPacketType pc = static_cast<HCIPacketType>( jau::get_uint8(buffer + 0) );

    if( HCIPacketType::EVENT != pc ) {
        return nullptr;
    }

    const jau::nsize_t paramSize = buffer_size >= number(HCIConstSizeT::EVENT_HDR_SIZE) ? jau::get_uint8(buffer + 2) : 0;
    if( buffer_size < number(HCIConstSizeT::EVENT_HDR_SIZE) + paramSize ) {
        jau_WARN_PRINT("HCIEvent::getSpecialized: length mismatch %zu < EVENT_HDR_SIZE(%zu) + %zu",
                buffer_size, *(HCIConstSizeT::EVENT_HDR_SIZE), paramSize);
        return nullptr;
    }

    try {
        const HCIEventType ec = static_cast<HCIEventType>( jau::get_uint8(buffer + 1) );
        switch( ec ) {
            case HCIEventType::DISCONN_COMPLETE:
                return std::make_unique<HCIDisconnectionCompleteEvent>(buffer, buffer_size);
            case HCIEventType::CMD_COMPLETE:
                return std::make_unique<HCICommandCompleteEvent>(buffer, buffer_size);
            case HCIEventType::CMD_STATUS:
                return std::make_unique<HCICommandStatusEvent>(buffer, buffer_size);
            case HCIEventType::LE_META: {
                const HCIMetaEventType mec = static_cast<HCIMetaEventType>( jau::get_uint8(buffer + number(HCIConstSizeT::EVENT_HDR_SIZE)) );
                switch( mec ) {
                    case HCIMetaEventType::LE_LTK_REQUEST:
                        return std::make_unique<HCILELTKReqEvent>(buffer, buffer_size);
                    default:
                        // May use HCIStructCmdCompleteMetaEvt template based on HCIMetaEvent.
                        return std::make_unique<HCIMetaEvent>(buffer, buffer_size, 1);
                }
            }
            default:
                // No further specialization, use HCIStructCmdCompleteEvt template
                return std::make_unique<HCIEvent>(buffer, buffer_size, 0);
        }
    } catch (...) {
        jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
    }
    return nullptr;
}

std::string HCIPacket::toString() const noexcept {
    return jau_format_string("%s[%s, %s]", nameString(), baseString(), valueString());
}

std::string HCICommand::baseString() const noexcept {
    return jau_format_string("opcode=%#x %s", *getOpcode(), getOpcode());
}

std::string HCICommand::valueString() const noexcept {
    const jau::nsize_t psz = getParamSize();
    return jau_format_string("param[size %zu, data %s], tsx %zu", getParamSize(),
        (psz > 0 ? jau::toHexString(getParam(), psz, jau::lb_endian_t::little) : ""),
        getTotalSize());
}

std::string HCILEEnableEncryptionCmd::valueString() const noexcept {
    return jau_format_string("data[handle %#x, rand %s, ediv %s, ltk %s], tsz %zu",
        getHandle(),
        jau::toHexString(pdu.get_ptr_nc(number(HCIConstSizeT::COMMAND_HDR_SIZE) + 2), 8, jau::lb_endian_t::big),
        jau::toHexString(pdu.get_ptr_nc(number(HCIConstSizeT::COMMAND_HDR_SIZE) + 2 + 8), 2, jau::lb_endian_t::big),
        jau::toHexString(pdu.get_ptr_nc(number(HCIConstSizeT::COMMAND_HDR_SIZE) + 2 + 8 + 2), 16, jau::lb_endian_t::little),
        getTotalSize());
}

std::string HCILELTKReplyAckCmd::valueString() const noexcept {
    return jau_format_string("data[handle %#x, ltk %s], tsz %zu",
        getHandle(),
        jau::toHexString(pdu.get_ptr_nc(number(HCIConstSizeT::COMMAND_HDR_SIZE) + 2), 16, jau::lb_endian_t::little),
        getTotalSize());
}

std::string HCILELTKReplyRejCmd::valueString() const noexcept {
    return jau_format_string("data[handle %#x], tsz %zu", getHandle(), getTotalSize());
}

std::string HCIACLData::toString() const noexcept {
    const uint8_t* l2cap_data;
    return jau_format_string("ACLData[size %zu, data %s, tsz %zu]",
        getParamSize(), getL2CapFrame(l2cap_data).toString(l2cap_data), getTotalSize());
}

std::string HCIACLData::toString(const L2CapFrame& l2cap, const uint8_t* l2cap_data) const noexcept {
    return jau_format_string("ACLData[size %zu, data %s, tsz %zu]",
        getParamSize(), l2cap.toString(l2cap_data), getTotalSize());
}

std::string HCIEvent::baseString() const noexcept {
    const HCIEventType v = getEventType();
    return jau_format_string("event=%#x %s", *v, v);
}

std::string HCIEvent::valueString() const noexcept {
    const jau::nsize_t d_sz_base = getBaseParamSize();
    const jau::nsize_t d_sz = getParamSize();
    return jau_format_string("data[size %zu/%zu, data %s], tsz %zu", d_sz, d_sz_base,
        (d_sz > 0 ? jau::toHexString(getParam(), d_sz, jau::lb_endian_t::little) : ""),
        getTotalSize());
}

std::string HCIDisconnectionCompleteEvent::baseString() const noexcept {
    const HCIStatusCode v = getReason();
    return jau_format_string("%s, status %s, handle %#x, reason %#x %s", HCIEvent::baseString(),
        jau::toHexString(static_cast<uint8_t>(getStatus()))+" "+to_string(getStatus()),
        getHandle(), *v, v);
}

std::string HCICommandCompleteEvent::baseString() const noexcept {
    const HCIOpcode opc = getOpcode();
    return jau_format_string("%s, opcode=%#x %s, ncmd %u",
        HCIEvent::baseString(), *opc, opc, getNumCommandPackets());
}

std::string HCICommandStatusEvent::baseString() const noexcept {
    const HCIOpcode opc = getOpcode();
    const HCIStatusCode s = getStatus();
    return jau_format_string("%s, opcode=%#x %s, ncmd %u, status %#x %s",
        HCIEvent::baseString(), *opc, opc, getNumCommandPackets(), *s, s);
}

std::string HCIMetaEvent::baseString() const noexcept {
    const HCIMetaEventType v = getMetaEventType();
    return jau_format_string("event=%#x %s (le-meta)", *v, v);
}

std::string HCILELTKReqEvent::valueString() const noexcept {
    return jau_format_string("data[handle %#x, rand %s, ediv %s], tsz %zu", getHandle(),
        jau::toHexString(pdu.get_ptr_nc(number(HCIConstSizeT::EVENT_HDR_SIZE) + 1+2),   8, jau::lb_endian_t::big),
        jau::toHexString(pdu.get_ptr_nc(number(HCIConstSizeT::EVENT_HDR_SIZE) + 1+2+8), 2, jau::lb_endian_t::big),
        getTotalSize());
}

std::string HCILocalVersion::toString() const noexcept {
    return jau_format_string("LocalVersion[version %u.%u, manuf %#x, lmp %u.%u]",
        hci_ver, hci_rev, manufacturer, lmp_ver, lmp_subver);
}

std::string L2CapFrame::toString() const noexcept {
    return jau_format_string("l2cap[handle %#x, flags[pb %s, bc %u], cid %s, psm %s, len %u]",
        handle, pb_flag, bc_flag, cid, psm, len);
}

std::string L2CapFrame::toString(const uint8_t* l2cap_data) const noexcept {
    return jau_format_string("l2cap[handle %#x, flags[pb %s, bc %u], cid %s, psm %s, len %u, data %s]",
        handle, pb_flag, bc_flag, cid, psm, len,
        (nullptr != l2cap_data && 0 < len ?  jau::toHexString(l2cap_data, len, jau::lb_endian_t::little) : "empty"));
}

std::unique_ptr<HCIACLData> HCIACLData::getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept {
    const HCIPacketType pc = static_cast<HCIPacketType>( jau::get_uint8(buffer + 0) );
    if( HCIPacketType::ACLDATA != pc ) {
        return nullptr;
    }
    const jau::nsize_t paramSize = buffer_size >= number(HCIConstSizeT::ACL_HDR_SIZE) ? jau::get_uint16(buffer + 3) : 0;
    if( buffer_size < number(HCIConstSizeT::ACL_HDR_SIZE) + paramSize ) {
        if( jau::environment::get().verbose ) {
            jau_WARN_PRINT("HCIACLData::getSpecialized: length mismatch %zu < ACL_HDR_SIZE(%zu) + %zu",
                    buffer_size, *(HCIConstSizeT::ACL_HDR_SIZE), paramSize);
        }
        return nullptr;
    }
    try {
        return std::make_unique<HCIACLData>(buffer, buffer_size);
    } catch (...) {
        jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
    }
    return nullptr;
}

__pack ( struct l2cap_hdr { // NOLINT(misc-use-internal-linkage)
    uint16_t len;
    uint16_t cid;
} );

L2CapFrame HCIACLData::getL2CapFrame(const uint8_t* & l2cap_data) const noexcept {
    const uint16_t h_f = getHandleAndFlags();
    uint16_t size = static_cast<uint16_t>(getParamSize());
    const uint8_t * data = getParam();
    const uint16_t handle = get_handle(h_f);
    const L2CapFrame::PBFlag pb_flag { get_pbflag(h_f) };
    const uint8_t bc_flag = get_bcflag(h_f);
    const l2cap_hdr* hdr = reinterpret_cast<const l2cap_hdr*>(data);

    l2cap_data = nullptr;

    switch( pb_flag ) {
        case L2CapFrame::PBFlag::START_NON_AUTOFLUSH_HOST:
            [[fallthrough]];
        case L2CapFrame::PBFlag::START_AUTOFLUSH:
            [[fallthrough]];
        case L2CapFrame::PBFlag::COMPLETE_L2CAP_AUTOFLUSH:
        {
            if( size < sizeof(*hdr) ) {
                jau_COND_PRINT(HCIEnv::get().DEBUG_EVENT, "l2cap DROP frame-size %u < hdr-size %zu, handle %u", size, sizeof(*hdr), handle);
                return L2CapFrame { handle, pb_flag, bc_flag, L2CAP_CID::UNDEFINED, L2CAP_PSM::UNDEFINED, 0 };
            }
            const uint16_t len = jau::le_to_cpu(hdr->len);
            const L2CAP_CID cid = L2CAP_CID( jau::le_to_cpu(hdr->cid) );
            data += sizeof(*hdr);
            size -= sizeof(*hdr);
            if( len <= size ) { // tolerate frame size > len, cutting-off excess octets
                l2cap_data = data;
                return L2CapFrame { handle, pb_flag, bc_flag, cid, L2CAP_PSM::UNDEFINED, len };
            } else {
                jau_COND_PRINT(HCIEnv::get().DEBUG_EVENT, "l2cap DROP frame-size %u < l2cap-size %u, handle %u", size, len, handle);
                return L2CapFrame { handle, pb_flag, bc_flag, L2CAP_CID::UNDEFINED, L2CAP_PSM::UNDEFINED, 0 };
            }
        } break;

        case L2CapFrame::PBFlag::CONTINUING_FRAGMENT:
            [[fallthrough]];
        default: // not supported
            jau_COND_PRINT(HCIEnv::get().DEBUG_EVENT, "l2cap DROP frame flag 0x%2.2x not supported, handle %u, packet-size %u", *pb_flag, handle, size);
            return L2CapFrame { handle, pb_flag, bc_flag, L2CAP_CID::UNDEFINED, L2CAP_PSM::UNDEFINED, 0 };
    }
}

} /* namespace direct_bt */
