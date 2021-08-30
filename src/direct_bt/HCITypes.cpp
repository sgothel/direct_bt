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

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>
#include <cstdio>

#include <algorithm>

#include <jau/ringbuffer.hpp>

#include "HCITypes.hpp"

extern "C" {
    #include <inttypes.h>
    #include <unistd.h>
    #include <sys/param.h>
    #include <sys/uio.h>
    #include <sys/types.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <poll.h>
}

namespace direct_bt {

#define HCI_STATUS_CODE(X) \
        X(SUCCESS) \
        X(UNKNOWN_HCI_COMMAND) \
        X(UNKNOWN_CONNECTION_IDENTIFIER) \
        X(HARDWARE_FAILURE) \
        X(PAGE_TIMEOUT) \
        X(AUTHENTICATION_FAILURE) \
        X(PIN_OR_KEY_MISSING) \
        X(MEMORY_CAPACITY_EXCEEDED) \
        X(CONNECTION_TIMEOUT) \
        X(CONNECTION_LIMIT_EXCEEDED) \
        X(SYNC_DEVICE_CONNECTION_LIMIT_EXCEEDED) \
        X(CONNECTION_ALREADY_EXISTS) \
        X(COMMAND_DISALLOWED) \
        X(CONNECTION_REJECTED_LIMITED_RESOURCES) \
        X(CONNECTION_REJECTED_SECURITY) \
        X(CONNECTION_REJECTED_UNACCEPTABLE_BD_ADDR) \
        X(CONNECTION_ACCEPT_TIMEOUT_EXCEEDED) \
        X(UNSUPPORTED_FEATURE_OR_PARAM_VALUE) \
        X(INVALID_HCI_COMMAND_PARAMETERS) \
        X(REMOTE_USER_TERMINATED_CONNECTION) \
        X(REMOTE_DEVICE_TERMINATED_CONNECTION_LOW_RESOURCES) \
        X(REMOTE_DEVICE_TERMINATED_CONNECTION_POWER_OFF) \
        X(CONNECTION_TERMINATED_BY_LOCAL_HOST) \
        X(REPEATED_ATTEMPTS) \
        X(PAIRING_NOT_ALLOWED) \
        X(UNKNOWN_LMP_PDU) \
        X(UNSUPPORTED_REMOTE_OR_LMP_FEATURE) \
        X(SCO_OFFSET_REJECTED) \
        X(SCO_INTERVAL_REJECTED) \
        X(SCO_AIR_MODE_REJECTED) \
        X(INVALID_LMP_OR_LL_PARAMETERS) \
        X(UNSPECIFIED_ERROR) \
        X(UNSUPPORTED_LMP_OR_LL_PARAMETER_VALUE) \
        X(ROLE_CHANGE_NOT_ALLOWED) \
        X(LMP_OR_LL_RESPONSE_TIMEOUT) \
        X(LMP_OR_LL_COLLISION) \
        X(LMP_PDU_NOT_ALLOWED) \
        X(ENCRYPTION_MODE_NOT_ACCEPTED) \
        X(LINK_KEY_CANNOT_BE_CHANGED) \
        X(REQUESTED_QOS_NOT_SUPPORTED) \
        X(INSTANT_PASSED) \
        X(PAIRING_WITH_UNIT_KEY_NOT_SUPPORTED) \
        X(DIFFERENT_TRANSACTION_COLLISION) \
        X(QOS_UNACCEPTABLE_PARAMETER) \
        X(QOS_REJECTED) \
        X(CHANNEL_ASSESSMENT_NOT_SUPPORTED) \
        X(INSUFFICIENT_SECURITY) \
        X(PARAMETER_OUT_OF_RANGE) \
        X(ROLE_SWITCH_PENDING) \
        X(RESERVED_SLOT_VIOLATION) \
        X(ROLE_SWITCH_FAILED) \
        X(EIR_TOO_LARGE) \
        X(SIMPLE_PAIRING_NOT_SUPPORTED_BY_HOST) \
        X(HOST_BUSY_PAIRING) \
        X(CONNECTION_REJECTED_NO_SUITABLE_CHANNEL) \
        X(CONTROLLER_BUSY) \
        X(UNACCEPTABLE_CONNECTION_PARAM) \
        X(ADVERTISING_TIMEOUT) \
        X(CONNECTION_TERMINATED_MIC_FAILURE) \
        X(CONNECTION_EST_FAILED_OR_SYNC_TIMEOUT) \
        X(MAX_CONNECTION_FAILED) \
        X(COARSE_CLOCK_ADJ_REJECTED) \
        X(TYPE0_SUBMAP_NOT_DEFINED) \
        X(UNKNOWN_ADVERTISING_IDENTIFIER) \
        X(LIMIT_REACHED) \
        X(OPERATION_CANCELLED_BY_HOST) \
        X(PACKET_TOO_LONG) \
        X(FAILED) \
        X(CONNECT_FAILED) \
        X(AUTH_FAILED) \
        X(NOT_PAIRED) \
        X(NO_RESOURCES) \
        X(TIMEOUT) \
        X(ALREADY_CONNECTED) \
        X(BUSY) \
        X(REJECTED) \
        X(NOT_SUPPORTED) \
        X(INVALID_PARAMS) \
        X(DISCONNECTED) \
        X(NOT_POWERED) \
        X(CANCELLED) \
        X(INVALID_INDEX) \
        X(RFKILLED) \
        X(ALREADY_PAIRED) \
        X(PERMISSION_DENIED) \
        X(INTERNAL_TIMEOUT) \
        X(INTERNAL_FAILURE) \
        X(UNKNOWN)

#define HCI_STATUS_CODE_CASE_TO_STRING(V) case HCIStatusCode::V: return #V;

std::string to_string(const HCIStatusCode ec) noexcept {
    switch(ec) {
    HCI_STATUS_CODE(HCI_STATUS_CODE_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown HCIStatusCode";
}

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

#define HCI_OPCODE(X) \
    X(SPECIAL) \
    X(CREATE_CONN) \
    X(DISCONNECT) \
    X(IO_CAPABILITY_REQ_REPLY) \
    X(IO_CAPABILITY_REQ_NEG_REPLY) \
    X(SET_EVENT_MASK) \
    X(RESET) \
    X(READ_LOCAL_VERSION) \
    X(READ_LOCAL_COMMANDS) \
    X(LE_SET_EVENT_MASK) \
    X(LE_READ_BUFFER_SIZE) \
    X(LE_READ_LOCAL_FEATURES) \
    X(LE_SET_RANDOM_ADDR) \
    X(LE_SET_ADV_PARAM) \
    X(LE_READ_ADV_TX_POWER) \
    X(LE_SET_ADV_DATA) \
    X(LE_SET_SCAN_RSP_DATA) \
    X(LE_SET_ADV_ENABLE) \
    X(LE_SET_SCAN_PARAM) \
    X(LE_SET_SCAN_ENABLE) \
    X(LE_CREATE_CONN) \
    X(LE_CREATE_CONN_CANCEL) \
    X(LE_READ_WHITE_LIST_SIZE) \
    X(LE_CLEAR_WHITE_LIST) \
    X(LE_ADD_TO_WHITE_LIST) \
    X(LE_DEL_FROM_WHITE_LIST) \
    X(LE_CONN_UPDATE) \
    X(LE_READ_REMOTE_FEATURES) \
    X(LE_ENABLE_ENC) \
    X(LE_READ_PHY) \
    X(LE_SET_DEFAULT_PHY) \
    X(LE_SET_EXT_SCAN_PARAMS) \
    X(LE_SET_EXT_SCAN_ENABLE) \
    X(LE_EXT_CREATE_CONN)


#define HCI_OPCODE_CASE_TO_STRING(V) case HCIOpcode::V: return #V;

std::string to_string(const HCIOpcode op) noexcept {
    switch(op) {
    HCI_OPCODE(HCI_OPCODE_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown HCIOpcode";
}

#define HCI_EVENTTYPE(X) \
    X(INVALID) \
    X(INQUIRY_COMPLETE) \
    X(INQUIRY_RESULT) \
    X(CONN_COMPLETE) \
    X(CONN_REQUEST) \
    X(DISCONN_COMPLETE) \
    X(AUTH_COMPLETE) \
    X(REMOTE_NAME) \
    X(ENCRYPT_CHANGE) \
    X(CHANGE_LINK_KEY_COMPLETE) \
    X(REMOTE_FEATURES) \
    X(REMOTE_VERSION) \
    X(QOS_SETUP_COMPLETE) \
    X(CMD_COMPLETE) \
    X(CMD_STATUS) \
    X(HARDWARE_ERROR) \
    X(ROLE_CHANGE) \
    X(NUM_COMP_PKTS) \
    X(MODE_CHANGE) \
    X(PIN_CODE_REQ) \
    X(LINK_KEY_REQ) \
    X(LINK_KEY_NOTIFY) \
    X(CLOCK_OFFSET) \
    X(PKT_TYPE_CHANGE) \
    X(ENCRYPT_KEY_REFRESH_COMPLETE) \
    X(IO_CAPABILITY_REQUEST) \
    X(IO_CAPABILITY_RESPONSE) \
    X(LE_META) \
    X(DISCONN_PHY_LINK_COMPLETE) \
    X(DISCONN_LOGICAL_LINK_COMPLETE) \
    X(AMP_Receiver_Report)

#define HCI_EVENTTYPE_CASE_TO_STRING(V) case HCIEventType::V: return #V;

std::string to_string(const HCIEventType op) noexcept {
        switch(op) {
        HCI_EVENTTYPE(HCI_EVENTTYPE_CASE_TO_STRING)
            default: ; // fall through intended
        }
        return "Unknown HCIEventType";
}

#define HCI_METATYPE(X) \
    X(INVALID) \
    X(LE_CONN_COMPLETE) \
    X(LE_ADVERTISING_REPORT) \
    X(LE_CONN_UPDATE_COMPLETE) \
    X(LE_REMOTE_FEAT_COMPLETE) \
    X(LE_LTKEY_REQUEST) \
    X(LE_REMOTE_CONN_PARAM_REQ) \
    X(LE_DATA_LENGTH_CHANGE) \
    X(LE_READ_LOCAL_P256_PUBKEY_COMPLETE) \
    X(LE_GENERATE_DHKEY_COMPLETE) \
    X(LE_EXT_CONN_COMPLETE) \
    X(LE_DIRECT_ADV_REPORT) \
    X(LE_PHY_UPDATE_COMPLETE) \
    X(LE_EXT_ADV_REPORT) \
    X(LE_PERIODIC_ADV_SYNC_ESTABLISHED) \
    X(LE_PERIODIC_ADV_REPORT) \
    X(LE_PERIODIC_ADV_SYNC_LOST) \
    X(LE_SCAN_TIMEOUT) \
    X(LE_ADV_SET_TERMINATED) \
    X(LE_SCAN_REQ_RECEIVED) \
    X(LE_CHANNEL_SEL_ALGO) \
    X(LE_CONNLESS_IQ_REPORT) \
    X(LE_CONN_IQ_REPORT) \
    X(LE_CTE_REQ_FAILED) \
    X(LE_PERIODIC_ADV_SYNC_TRANSFER_RECV) \
    X(LE_CIS_ESTABLISHED) \
    X(LE_CIS_REQUEST) \
    X(LE_CREATE_BIG_COMPLETE) \
    X(LE_TERMINATE_BIG_COMPLETE) \
    X(LE_BIG_SYNC_ESTABLISHED) \
    X(LE_BIG_SYNC_LOST) \
    X(LE_REQUEST_PEER_SCA_COMPLETE) \
    X(LE_PATH_LOSS_THRESHOLD) \
    X(LE_TRANSMIT_POWER_REPORTING) \
    X(LE_BIGINFO_ADV_REPORT)

#define HCI_METATYPE_CASE_TO_STRING(V) case HCIMetaEventType::V: return #V;

std::string to_string(const HCIMetaEventType op) noexcept {
    switch(op) {
    HCI_METATYPE(HCI_METATYPE_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown HCIMetaType";
}

std::unique_ptr<HCIEvent> HCIEvent::getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept {
    const HCIPacketType pc = static_cast<HCIPacketType>( jau::get_uint8(buffer, 0) );
    if( HCIPacketType::EVENT != pc ) {
        return nullptr;
    }
    const jau::nsize_t paramSize = buffer_size >= number(HCIConstSizeT::EVENT_HDR_SIZE) ? jau::get_uint8(buffer, 2) : 0;
    if( buffer_size < number(HCIConstSizeT::EVENT_HDR_SIZE) + paramSize ) {
        WARN_PRINT("HCIEvent::getSpecialized: length mismatch %u < EVENT_HDR_SIZE(%u) + %u",
                buffer_size, number(HCIConstSizeT::EVENT_HDR_SIZE), paramSize);
        return nullptr;
    }

    const HCIEventType ec = static_cast<HCIEventType>( jau::get_uint8(buffer, 1) );
    switch( ec ) {
        case HCIEventType::DISCONN_COMPLETE:
            return std::make_unique<HCIDisconnectionCompleteEvent>(buffer, buffer_size);
        case HCIEventType::CMD_COMPLETE:
            return std::make_unique<HCICommandCompleteEvent>(buffer, buffer_size);
        case HCIEventType::CMD_STATUS:
            return std::make_unique<HCICommandStatusEvent>(buffer, buffer_size);
        case HCIEventType::LE_META:
            // No need to HCIMetaType specializations as we use HCIStructCmdCompleteMetaEvt template
            // based on HCIMetaEvent.
            return std::make_unique<HCIMetaEvent>(buffer, buffer_size, 1);
        default:
            // No further specialization, use HCIStructCmdCompleteEvt template
            return std::make_unique<HCIEvent>(buffer, buffer_size, 0);
    }
}

std::string HCILocalVersion::toString() noexcept {
    return "LocalVersion[version "+std::to_string(hci_ver)+"."+std::to_string(hci_rev)+
           ", manuf "+jau::to_hexstring(manufacturer)+", lmp "+std::to_string(lmp_ver)+"."+std::to_string(lmp_subver)+"]";
}

std::string HCIACLData::l2cap_frame::toString(const PBFlag v) noexcept {
    switch( v ) {
        case HCIACLData::l2cap_frame::PBFlag::START_NON_AUTOFLUSH_HOST: return "START_NON_AUTOFLUSH_HOST";
        case HCIACLData::l2cap_frame::PBFlag::CONTINUING_FRAGMENT:      return "CONTINUING_FRAGMENT";
        case HCIACLData::l2cap_frame::PBFlag::START_AUTOFLUSH:          return "START_AUTOFLUSH";
        case HCIACLData::l2cap_frame::PBFlag::COMPLETE_L2CAP_AUTOFLUSH: return "COMPLETE_L2CAP_AUTOFLUSH";
        default:                                                        return "Unknown PBFlag";
    }
}

std::unique_ptr<HCIACLData> HCIACLData::getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept {
    const HCIPacketType pc = static_cast<HCIPacketType>( jau::get_uint8(buffer, 0) );
    if( HCIPacketType::ACLDATA != pc ) {
        return nullptr;
    }
    const jau::nsize_t paramSize = buffer_size >= number(HCIConstSizeT::ACL_HDR_SIZE) ? jau::get_uint16(buffer, 3) : 0;
    if( buffer_size < number(HCIConstSizeT::ACL_HDR_SIZE) + paramSize ) {
        if( jau::environment::get().verbose ) {
            WARN_PRINT("HCIACLData::getSpecialized: length mismatch %u < ACL_HDR_SIZE(%u) + %u",
                    buffer_size, number(HCIConstSizeT::ACL_HDR_SIZE), paramSize);
        }
        return nullptr;
    }
    return std::make_unique<HCIACLData>(buffer, buffer_size);
}

__pack ( struct l2cap_hdr {
    uint16_t len;
    uint16_t cid;
} );

HCIACLData::l2cap_frame HCIACLData::getL2CAPFrame(const uint8_t* & l2cap_data) const noexcept {
    const uint16_t h_f = getHandleAndFlags();
    uint16_t size = static_cast<uint16_t>(getParamSize());
    const uint8_t * data = getParam();
    const uint16_t handle = get_handle(h_f);
    const HCIACLData::l2cap_frame::PBFlag pb_flag { get_pbflag(h_f) };
    const uint8_t bc_flag = get_bcflag(h_f);
    const l2cap_hdr* hdr = reinterpret_cast<const l2cap_hdr*>(data);

    l2cap_data = nullptr;

    switch( pb_flag ) {
        case HCIACLData::l2cap_frame::PBFlag::START_NON_AUTOFLUSH_HOST:
            [[fallthrough]];
        case HCIACLData::l2cap_frame::PBFlag::START_AUTOFLUSH:
            [[fallthrough]];
        case HCIACLData::l2cap_frame::PBFlag::COMPLETE_L2CAP_AUTOFLUSH:
        {
            if( size < sizeof(*hdr) ) {
                DBG_PRINT("l2cap DROP frame-size %d < hdr-size %z, handle ", size, sizeof(*hdr), handle);
                return l2cap_frame { handle, pb_flag, bc_flag, L2CAP_CID::UNDEFINED, L2CAP_PSM::UNDEFINED, 0 };
            }
            const uint16_t len = jau::le_to_cpu(hdr->len);
            const L2CAP_CID cid = L2CAP_CID( jau::le_to_cpu(hdr->cid) );
            data += sizeof(*hdr);
            size -= sizeof(*hdr);
            if( len <= size ) { // tolerate frame size > len, cutting-off excess octets
                l2cap_data = data;
                return l2cap_frame { handle, pb_flag, bc_flag, cid, L2CAP_PSM::UNDEFINED, len };
            } else {
                DBG_PRINT("l2cap DROP frame-size %d < l2cap-size %d, handle ", size, len, handle);
                return l2cap_frame { handle, pb_flag, bc_flag, L2CAP_CID::UNDEFINED, L2CAP_PSM::UNDEFINED, 0 };
            }
        } break;

        case HCIACLData::l2cap_frame::PBFlag::CONTINUING_FRAGMENT:
            [[fallthrough]];
        default: // not supported
            DBG_PRINT("l2cap DROP frame flag 0x%2.2x not supported, handle %d, packet-size %d", pb_flag, handle, size);
            return l2cap_frame { handle, pb_flag, bc_flag, L2CAP_CID::UNDEFINED, L2CAP_PSM::UNDEFINED, 0 };
    }
}

} /* namespace direct_bt */
