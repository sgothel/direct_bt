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
#include <vector>
#include <cstdio>

// #define PERF_PRINT_ON 1
#include <jau/debug.hpp>

#include "MgmtTypes.hpp"
#include "BTTypes1.hpp"
#include "jau/cpp_lang_util.hpp"
#include "jau/string_util.hpp"

extern "C" {
    #include <unistd.h>
}

using namespace direct_bt;

// *************************************************
// *************************************************
// *************************************************

namespace direct_bt {
    JAU_MAKE_ENUM_STRING_CODE(MgmtStatus,
        SUCCESS, UNKNOWN_COMMAND, NOT_CONNECTED, FAILED, CONNECT_FAILED, AUTH_FAILED, NOT_PAIRED, NO_RESOURCES, TIMEOUT,
        ALREADY_CONNECTED, BUSY, REJECTED, NOT_SUPPORTED, INVALID_PARAMS, DISCONNECTED, NOT_POWERED, CANCELLED,
        INVALID_INDEX, RFKILLED, ALREADY_PAIRED, PERMISSION_DENIED);

    JAU_MAKE_ENUM_STRING_CODE(MgmtLinkKeyType,
        COMBI, LOCAL_UNIT, REMOTE_UNIT, DBG_COMBI, UNAUTH_COMBI_P192, AUTH_COMBI_P192, CHANGED_COMBI, UNAUTH_COMBI_P256, AUTH_COMBI_P256, NONE);

    JAU_MAKE_ENUM_STRING_CODE(MgmtLTKType,
        UNAUTHENTICATED, AUTHENTICATED, UNAUTHENTICATED_P256, AUTHENTICATED_P256, DEBUG_P256, NONE);

    JAU_MAKE_ENUM_STRING_CODE(MgmtCSRKType,
        UNAUTHENTICATED_LOCAL, UNAUTHENTICATED_REMOTE, AUTHENTICATED_LOCAL, AUTHENTICATED_REMOTE, NONE);

    JAU_MAKE_ENUM_STRING2_CODE(MgmtCommand::Opcode, Opcode,
        READ_VERSION, READ_COMMANDS, READ_INDEX_LIST, READ_INFO, SET_POWERED, SET_DISCOVERABLE, SET_CONNECTABLE, SET_FAST_CONNECTABLE,
        SET_BONDABLE, SET_LINK_SECURITY, SET_SSP, SET_HS, SET_LE, SET_DEV_CLASS, SET_LOCAL_NAME, ADD_UUID, REMOVE_UUID, LOAD_LINK_KEYS,
        LOAD_LONG_TERM_KEYS, DISCONNECT, GET_CONNECTIONS, PIN_CODE_REPLY, PIN_CODE_NEG_REPLY, SET_IO_CAPABILITY, PAIR_DEVICE, CANCEL_PAIR_DEVICE,
        UNPAIR_DEVICE, USER_CONFIRM_REPLY, USER_CONFIRM_NEG_REPLY, USER_PASSKEY_REPLY, USER_PASSKEY_NEG_REPLY, READ_LOCAL_OOB_DATA,
        ADD_REMOTE_OOB_DATA, REMOVE_REMOTE_OOB_DATA, START_DISCOVERY, STOP_DISCOVERY, CONFIRM_NAME, BLOCK_DEVICE, UNBLOCK_DEVICE, SET_DEVICE_ID,
        SET_ADVERTISING, SET_BREDR, SET_STATIC_ADDRESS, SET_SCAN_PARAMS, SET_SECURE_CONN, SET_DEBUG_KEYS, SET_PRIVACY, LOAD_IRKS, GET_CONN_INFO,
        GET_CLOCK_INFO, ADD_DEVICE_WHITELIST, REMOVE_DEVICE_WHITELIST, LOAD_CONN_PARAM, READ_UNCONF_INDEX_LIST, READ_CONFIG_INFO,
        SET_EXTERNAL_CONFIG, SET_PUBLIC_ADDRESS, START_SERVICE_DISCOVERY, READ_LOCAL_OOB_EXT_DATA, READ_EXT_INDEX_LIST, READ_ADV_FEATURES,
        ADD_ADVERTISING, REMOVE_ADVERTISING, GET_ADV_SIZE_INFO, START_LIMITED_DISCOVERY, READ_EXT_INFO, SET_APPEARANCE, GET_PHY_CONFIGURATION,
        SET_PHY_CONFIGURATION, SET_BLOCKED_KEYS, SET_WIDEBAND_SPEECH, READ_SECURITY_INFO, READ_EXP_FEATURES_INFO, SET_EXP_FEATURE,
        READ_DEF_SYSTEM_CONFIG, SET_DEF_SYSTEM_CONFIG, READ_DEF_RUNTIME_CONFIG, SET_DEF_RUNTIME_CONFIG, GET_DEVICE_FLAGS, SET_DEVICE_FLAGS,
        READ_ADV_MONITOR_FEATURES, ADD_ADV_PATTERNS_MONITOR, REMOVE_ADV_MONITOR);

    JAU_MAKE_ENUM_STRING2_CODE(MgmtDefaultParam::Type, Type,
        BREDR_PAGE_SCAN_TYPE, BREDR_PAGE_SCAN_INTERVAL, BREDR_PAGE_SCAN_WINDOW, BREDR_INQUIRY_TYPE, BREDR_INQUIRY_INTERVAL, BREDR_INQUIRY_WINDOW,
        BREDR_LINK_SUPERVISOR_TIMEOUT, BREDR_PAGE_TIMEOUT, BREDR_MIN_SNIFF_INTERVAL, BREDR_MAX_SNIFF_INTERVAL, LE_ADV_MIN_INTERVAL,
        LE_ADV_MAX_INTERVAL, LE_MULTI_ADV_ROT_INTERVAL, LE_SCAN_INTERVAL_AUTOCONN, LE_SCAN_WINDOW_AUTOCONN, LE_SCAN_INTERVAL_WAKESCENARIO,
        LE_SCAN_WINDOW_WAKESCENARIO, LE_SCAN_INTERVAL_DISCOVERY, LE_SCAN_WINDOW_DISCOVERY, LE_SCAN_INTERVAL_ADVMON, LE_SCAN_WINDOW_ADVMON,
        LE_SCAN_INTERVAL_CONNECT, LE_SCAN_WINDOW_CONNECT, LE_MIN_CONN_INTERVAL, LE_MAX_CONN_INTERVAL, LE_CONN_LATENCY,
        LE_CONN_SUPERVISOR_TIMEOUT, LE_AUTOCONN_TIMEOUT, NONE);

    JAU_MAKE_ENUM_STRING2_CODE(MgmtEvent::Opcode, Opcode,
        INVALID, CMD_COMPLETE, CMD_STATUS, CONTROLLER_ERROR, INDEX_ADDED, INDEX_REMOVED, NEW_SETTINGS, CLASS_OF_DEV_CHANGED,
        LOCAL_NAME_CHANGED, NEW_LINK_KEY, NEW_LONG_TERM_KEY, DEVICE_CONNECTED, DEVICE_DISCONNECTED, CONNECT_FAILED,
        PIN_CODE_REQUEST, USER_CONFIRM_REQUEST, USER_PASSKEY_REQUEST, AUTH_FAILED, DEVICE_FOUND, DISCOVERING,
        DEVICE_BLOCKED, DEVICE_UNBLOCKED, DEVICE_UNPAIRED, PASSKEY_NOTIFY, NEW_IRK, NEW_CSRK, DEVICE_WHITELIST_ADDED,
        DEVICE_WHITELIST_REMOVED, NEW_CONN_PARAM, UNCONF_INDEX_ADDED, UNCONF_INDEX_REMOVED, NEW_CONFIG_OPTIONS,
        EXT_INDEX_ADDED, EXT_INDEX_REMOVED, LOCAL_OOB_DATA_UPDATED, ADVERTISING_ADDED, ADVERTISING_REMOVED,
        EXT_INFO_CHANGED, PHY_CONFIGURATION_CHANGED, EXP_FEATURE_CHANGED, DEVICE_FLAGS_CHANGED, ADV_MONITOR_ADDED,
        ADV_MONITOR_REMOVED, PAIR_DEVICE_COMPLETE, HCI_ENC_CHANGED, HCI_ENC_KEY_REFRESH_COMPLETE, HCI_LE_REMOTE_FEATURES,
        HCI_LE_PHY_UPDATE_COMPLETE, HCI_LE_LTK_REQUEST, HCI_LE_LTK_REPLY_ACK, HCI_LE_LTK_REPLY_REJ, HCI_LE_ENABLE_ENC
    );

    JAU_MAKE_ENUM_STRING2_CODE(MgmtEvtDeviceDisconnected::DisconnectReason, DisconnectReason,
        UNKNOWN, TIMEOUT, LOCAL_HOST, REMOTE, AUTH_FAILURE);
}

// *************************************************
// *************************************************
// *************************************************

std::string MgmtLongTermKey::toString() const noexcept { // hex-fmt aligned with btmon
    return jau_format_string("LTK[address[%s, %s%s], type %s, role %#x, enc_size %u, ediv %s, rand %s, ltk %s]",
        address, address_type, BDAddressAndType::getBLERandomAddressTypeString(address, address_type, ", "),
        key_type, role, enc_size,
        jau::toHexString(reinterpret_cast<const uint8_t *>(&ediv), sizeof(ediv), jau::lb_endian_t::little),
        jau::toHexString(reinterpret_cast<const uint8_t *>(&rand), sizeof(rand), jau::lb_endian_t::little),
        jau::toHexString(ltk.data, sizeof(ltk), jau::lb_endian_t::little));
}

std::string MgmtIdentityResolvingKey::toString() const noexcept {
    return jau_format_string("IRK[address[%s, %s%s], irk %s]",
        address, address_type, BDAddressAndType::getBLERandomAddressTypeString(address, address_type, ", "),
        jau::toHexString(irk.data, sizeof(irk), jau::lb_endian_t::little));
}

std::string MgmtSignatureResolvingKey::toString() const noexcept {
    return jau_format_string("CSRK[address[%s, %s%s], csrk %s]",
        address, address_type, BDAddressAndType::getBLERandomAddressTypeString(address, address_type, ", "),
        jau::toHexString(csrk.data, sizeof(csrk), jau::lb_endian_t::little));
}

std::string MgmtLinkKeyInfo::toString() const noexcept {
    return jau_format_string("LK[address[%s, %s%s], type %s, key %s, plen %u]",
        address, address_type, BDAddressAndType::getBLERandomAddressTypeString(address, address_type, ", "),
        key_type,
        jau::toHexString(key.data, sizeof(key), jau::lb_endian_t::little),
        pin_length);
}

std::string MgmtMsg::baseString() const noexcept {
    return jau_format_string("opcode %#x, dev_id %u", getIntOpcode(), getDevID());
}

std::string MgmtCommand::baseString() const noexcept {
    return jau_format_string("opcode %s, dev_id %u", getOpcode(), getDevID());
}

std::string MgmtCommand::valueString() const noexcept {
    const jau::nsize_t psz = getParamSize();
    return jau_format_string("param[size %zu, data '%s'], tsz %zu", psz,
        (psz > 0 ? jau::toHexString(getParam(), psz, jau::lb_endian_t::little) : ""),
        getTotalSize());
}

std::string MgmtCommand::toString() const noexcept {
    return jau_format_string("MgmtCmd[%s, %s]", baseString(), valueString());
}

std::string MgmtSetDiscoverableCmd::valueString() const noexcept {
    return jau_format_string("param[size %zu, data[state %#x, timeout %zus]], tsz %zu",
        getParamSize(), getDiscoverable(), getTimeout(), getTotalSize());
}

std::string MgmtSetLocalNameCmd::valueString() const noexcept {
    return jau_format_string("param[size %zu, data[name '%s', shortName '%s']], tsz %zu",
        getParamSize(), getName(), getShortName(), getTotalSize());
}

std::string MgmtLoadLinkKeyCmd::valueString() const noexcept {
    const jau::nsize_t keyCount = getKeyCount();
    std::string res = jau_format_string("param[size %zu, data[count %zu: ", getParamSize(), keyCount);
    for(jau::nsize_t i=0; i<keyCount; i++) {
        if( 0 < i ) {
            jau::append_string(res, ", ");
        }
        jau::do_noexcept([&]() { jau::append_string(res, getLinkKey(i).toString()); });
    }
    jau_append_string(res, "]], tsz %zu", getTotalSize());
    return res;
}

std::string MgmtLoadLongTermKeyCmd::valueString() const noexcept {
    const jau::nsize_t keyCount = getKeyCount();
    std::string res = jau_format_string("param[size %zu, data[count %zu: ", getParamSize(), keyCount);
    for(jau::nsize_t i=0; i<keyCount; i++) {
        if( 0 < i ) {
            jau::append_string(res, ", ");
        }
        jau::do_noexcept([&]() { jau::append_string(res, getLongTermKey(i).toString()); });
    }
    jau_append_string(res, "]], tsz %zu", getTotalSize());
    return res;
}

std::string MgmtIdentityResolveKeyCmd::valueString() const noexcept {
    const jau::nsize_t keyCount = getKeyCount();
    std::string res = jau_format_string("param[size %zu, data[count %zu: ", getParamSize(), keyCount);
    for(jau::nsize_t i=0; i<keyCount; i++) {
        if( 0 < i ) {
            jau::append_string(res, ", ");
        }
        jau::do_noexcept([&]() { jau::append_string(res, getLongTermKey(i).toString()); });
    }
    jau_append_string(res, "]], tsz %zu", getTotalSize());
    return res;
}

std::string MgmtCmdAdressInfoMeta::valueString() const noexcept {
    return jau_format_string("param[size %zu, data[address[%s, type %s]]], tsz %zu",
        getParamSize(), getAddress(), getAddressType(), getTotalSize());
}

std::string MgmtLoadIdentityResolvingKeyCmd::valueString() const noexcept {
    const jau::nsize_t keyCount = getKeyCount();
    std::string res = jau_format_string("param[size %zu, data[count %zu: ", getParamSize(), keyCount);
    for(jau::nsize_t i=0; i<keyCount; i++) {
        if( 0 < i ) {
            jau::append_string(res, ", ");
        }
        jau::do_noexcept([&]() { jau::append_string(res, getIdentityResolvingKey(i).toString()); });
    }
    jau_append_string(res, "]], tsz %zu", getTotalSize());
    return res;
}

std::string MgmtPinCodeReplyCmd::valueString() const noexcept {
    return jau_format_string("param[size %zu, data[address[%s, type %s], pin '%s']], tsz %zu",
        getParamSize(), getAddress(), getAddressType(), getPinCode(), getTotalSize());
}

std::string MgmtPairDeviceCmd::valueString() const noexcept {
    return jau_format_string("param[size %zu, data[address[%s, type %s], io %s]], tsz %zu",
        getParamSize(), getAddress(), getAddressType(), getIOCapability(), getTotalSize());
}

std::string MgmtUnpairDeviceCmd::valueString() const noexcept {
    return jau_format_string("param[size %zu, data[address[%s, type %s], disconnect %s]], tsz %zu",
        getParamSize(), getAddress(), getAddressType(), getDisconnect(), getTotalSize());
}

std::string MgmtUserPasskeyReplyCmd::valueString() const noexcept {
    return jau_format_string("param[size %zu, data[address[%s, type %s], passkey %u]], tsz %zu",
        getParamSize(), getAddress(), getAddressType(), getPasskey(), getTotalSize());
}

std::string MgmtAddDeviceToWhitelistCmd::valueString() const noexcept {
    return jau_format_string("param[size %zu, data[address[%s, type %s], connectionType %u]], tsz %zu",
        getParamSize(), getAddress(), getAddressType(), *getConnectionType(), getTotalSize());
}

std::string MgmtConnParam::toString() const noexcept {
    return jau_format_string("ConnParam[[address %s, type %s], interval[%u..%u], latency %u, timeout %u",
        address, address_type, min_interval, max_interval, latency, supervision_timeout);
}

std::string MgmtLoadConnParamCmd::valueString() const noexcept {
    const jau::nsize_t count = getParamCount();
    std::string res = jau_format_string("param[size %zu, data[count %zu: ", getParamSize(), count);
    for(jau::nsize_t i=0; i<count; i++) {
        if( 0 < i ) {
            jau::append_string(res, ", ");
        }
        jau::do_noexcept([&]() { jau::append_string(res, getConnParam(i).toString()); });
    }
    jau_append_string(res, "]], tsz %zu", getTotalSize());
    return res;
}

std::string MgmtDefaultParam::toString() const noexcept {
    std::string res = jau_format_string("%s (sz %zu): ", type, value.size());
    switch( value.size() ) {
        case 2: jau_append_string(res, "%u", value.get_uint16_nc(0) ); break;
        default: jau::append_string(res, value.toString());
    }
    return res;
}

std::string MgmtSetDefaultConnParamCmd::valueString() const noexcept {
    const jau::nsize_t count = 4;
    std::string res = jau_format_string("param[size %zu, data[count %zu: ", getParamSize(), count);
    for(jau::nsize_t i=0; i<count; i++) {
        if( 0 < i ) {
            jau::append_string(res, ", ");
        }
        jau::do_noexcept([&]() { jau::append_string(res, getDefaultParam(i).toString()); });
    }
    jau_append_string(res, "]], tsz %zu", getTotalSize());
    return res;
}

std::string MgmtEvent::baseString() const noexcept {
    return jau_format_string("opcode %s, dev_id %u", getOpcode(), getDevID());
}

std::string MgmtEvent::valueString() const noexcept {
    const jau::nsize_t d_sz = getDataSize();
    return jau_format_string("data[size %zu, data %s], tsz %zu", d_sz,
        (d_sz > 0 ? jau::toHexString(getData(), d_sz, jau::lb_endian_t::little) : ""),
        getTotalSize());
}

std::string MgmtEvent::toString() const noexcept {
    return jau_format_string("MgmtEvt[%s, %s]", baseString(), valueString());
}

std::string MgmtEvtAdressInfoMeta::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s]",
        MgmtEvent::baseString(), getAddress(), getAddressType());
}

std::string MgmtEvtCmdComplete::baseString() const noexcept {
    return jau_format_string("%s, cmd %s, status %#x %s",
        MgmtEvent::baseString(), getCmdOpcode(), *getStatus(), getStatus());
}

std::string MgmtEvtCmdStatus::baseString() const noexcept {
    return jau_format_string("%s, cmd %s, status %#x %s",
        MgmtEvent::baseString(), getCmdOpcode(), *getStatus(), getStatus());
}

std::string MgmtEvtControllerError::baseString() const noexcept {
    return jau_format_string("%s, error-code %#x", MgmtEvent::baseString(), getErrorCode());
}

std::string MgmtEvtNewSettings::baseString() const noexcept {
    return jau_format_string("%s, settings=%s", MgmtEvent::baseString(), getSettings());
}

std::string MgmtEvtLocalNameChanged::valueString() const noexcept {
    return jau_format_string("name '%s', shortName '%s'", getName(), getShortName());
}

std::string MgmtEvtNewLinkKey::baseString() const noexcept {
    return jau_format_string("%s, store %#x, %s", MgmtEvent::baseString(), getStoreHint(), getLinkKey());
}

std::string MgmtEvtNewLongTermKey::baseString() const noexcept {
    return jau_format_string("%s, store %#x, %s", MgmtEvent::baseString(), getStoreHint(), getLongTermKey());
}

std::string MgmtEvtDeviceConnected::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s], flags %#x, eir-sz %zu, hci_handle %#x",
        MgmtEvent::baseString(), getAddress(), getAddressType(),
        getFlags(), getEIRSize(), getHCIHandle());
}

std::string MgmtEvtDeviceDisconnected::baseString() const noexcept {
    const DisconnectReason v1 = getReason();
    const HCIStatusCode v2 = getHCIReason();
    return jau_format_string("%s, address[%s, type %s], reason[mgmt[%#x (%s)], hci[%#x (%s)]], hci_handle %#x",
        MgmtEvent::baseString(), getAddress(), getAddressType(),
        *v1, v1, *v2, v2, getHCIHandle());
}

std::string MgmtEvtDeviceConnectFailed::baseString() const noexcept {
    const MgmtStatus v1 = getStatus();
    const HCIStatusCode v2 = getHCIStatus();
    return jau_format_string("%s, address[%s, type %s], status[mgmt[%#x (%s)], hci[%#x (%s)]]",
        MgmtEvent::baseString(), getAddress(), getAddressType(),
        *v1, v1, *v2, v2);
}

std::string MgmtEvtPinCodeRequest::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s], secure %u",
        MgmtEvent::baseString(), getAddress(), getAddressType(), getSecure());
}

std::string MgmtEvtUserConfirmRequest::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s], confirm_hint %u, value %u",
        MgmtEvent::baseString(), getAddress(), getAddressType(), getConfirmHint(), getValue());
}

std::string MgmtEvtPasskeyNotify::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s], passkey %u, entered %u",
        MgmtEvent::baseString(), getAddress(), getAddressType(), getPasskey(), getEntered());
}

std::string MgmtEvtAuthFailed::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s], status %s",
        MgmtEvent::baseString(), getAddress(), getAddressType(), getStatus());
}

std::string MgmtEvtDeviceFound::baseString() const noexcept {
    if( nullptr != eireport ) {
        return jau_format_string("%s, %s", MgmtEvent::baseString(), eireport->toString(false /* includeServices */));
    } else {
        return jau_format_string("%s, address[%s, type %s], rssi %d, flags %#x, eir-sz %zu",
            MgmtEvent::baseString(), getAddress(), getAddressType(), getRSSI(), getFlags(), getEIRSize());
    }
}

std::string MgmtEvtDiscovering::baseString() const noexcept {
    return jau_format_string("%s, scan-type %s, enabled %s",
        MgmtEvent::baseString(), getScanType(), getEnabled());
}

std::string MgmtEvtNewIdentityResolvingKey::baseString() const noexcept {
    return jau_format_string("%s, store %#x, rnd_address %s, %s",
        MgmtEvent::baseString(), getStoreHint(), getRandomAddress(), getIdentityResolvingKey());
}

std::string MgmtEvtNewSignatureResolvingKey::baseString() const noexcept {
    return jau_format_string("%s, store %#x, %s",
        MgmtEvent::baseString(), getStoreHint(), getSignatureResolvingKey());
}

std::string MgmtEvtDeviceWhitelistAdded::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s], action %u",
        MgmtEvent::baseString(), getAddress(), getAddressType(), getAction());
}

std::string MgmtEvtNewConnectionParam::baseString() const noexcept {
    return jau_format_string("%s, store %#x, %s",
        MgmtEvent::baseString(), getStoreHint(), getConnParam());
}

std::string MgmtEvtPairDeviceComplete::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s], status %s",
        MgmtEvent::baseString(), getAddress(), getAddressType(), getStatus());
}

std::string MgmtEvtHCILERemoteFeatures::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s], status %s, features=%# " PRIx64,
        MgmtEvent::baseString(), getAddress(), getAddressType(), getHCIStatus(), *getFeatures());
}

std::string MgmtEvtHCILEPhyUpdateComplete::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s], status %s, Tx=%s, Rx=%s",
        MgmtEvent::baseString(), getAddress(), getAddressType(), getHCIStatus(), getTx(), getRx());
}

std::string MgmtEvtHCILELTKReq::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s], rand %s, ediv %s",
        MgmtEvent::baseString(), getAddress(), getAddressType(),
        jau::toHexString(pdu.get_ptr_nc(MGMT_HEADER_SIZE + 6+1),   8, jau::lb_endian_t::big),
        jau::toHexString(pdu.get_ptr_nc(MGMT_HEADER_SIZE + 6+1+8), 2, jau::lb_endian_t::big));

}

std::string MgmtEvtHCILELTKReplyAckCmd::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s], ltk %s",
        MgmtEvent::baseString(), getAddress(), getAddressType(),
        jau::toHexString(pdu.get_ptr_nc(MGMT_HEADER_SIZE + 6+1), 16, jau::lb_endian_t::little));
}

std::string MgmtEvtHCILELTKReplyRejCmd::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s]",
        MgmtEvent::baseString(), getAddress(), getAddressType());
}

std::string MgmtEvtHCILEEnableEncryptionCmd::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s], rand %s, ediv %s, ltk %s",
        MgmtEvent::baseString(), getAddress(), getAddressType(),
        jau::toHexString(pdu.get_ptr_nc(MGMT_HEADER_SIZE + 6+1),      8, jau::lb_endian_t::big),
        jau::toHexString(pdu.get_ptr_nc(MGMT_HEADER_SIZE + 6+1+8),    2, jau::lb_endian_t::big),
        jau::toHexString(pdu.get_ptr_nc(MGMT_HEADER_SIZE  + 6+1+8+2), 16, jau::lb_endian_t::little));
}

std::string MgmtEvtHCIEncryptionChanged::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s], status %s, enabled %#x",
        MgmtEvent::baseString(), getAddress(), getAddressType(), getHCIStatus(), getEncEnabled());
}

std::string MgmtEvtHCIEncryptionKeyRefreshComplete::baseString() const noexcept {
    return jau_format_string("%s, address[%s, type %s], status %s",
        MgmtEvent::baseString(), getAddress(), getAddressType(), getHCIStatus());
}

std::string MgmtEvtAdapterInfo::valueString() const noexcept {
    return jau_format_string("%s, version %u, manuf %u, settings[sup %s, cur %s], name '%s', shortName '%s'",
        getAddress(), getVersion(), getManufacturer(),
        getSupportedSetting(), getCurrentSetting(), getName(), getShortName());
}


std::string MgmtAdapterEventCallback::toString() const {
    return jau_format_string("MgmtAdapterEventCallback[dev_id %d, %s, %s]", dev_id, opc, callback);
}

// *************************************************
// *************************************************
// *************************************************

MgmtLTKType direct_bt::to_MgmtLTKType(const SMPLongTermKey::Property mask) noexcept {
    if( ( SMPLongTermKey::Property::AUTH & mask ) != SMPLongTermKey::Property::NONE ) {
        return ( SMPLongTermKey::Property::SC & mask ) != SMPLongTermKey::Property::NONE ?
                MgmtLTKType::AUTHENTICATED_P256 : MgmtLTKType::AUTHENTICATED;
    } else {
        return ( SMPLongTermKey::Property::SC & mask ) != SMPLongTermKey::Property::NONE ?
                MgmtLTKType::UNAUTHENTICATED_P256 : MgmtLTKType::UNAUTHENTICATED;
    }
}

// *************************************************
// *************************************************
// *************************************************

MgmtDefaultParam MgmtDefaultParam::read(const uint8_t* data, const jau::nsize_t length) {
    if( length < 2U ) {
        return MgmtDefaultParam();
    }
    const Type type = static_cast<Type>( jau::get_uint16(data + 0, jau::lb_endian_t::little) );
    if( length < 2U + 1U ) {
        return MgmtDefaultParam(type);
    }
    const uint8_t value_length = jau::get_uint8(data + 2);
    if( value_length != to_size(type) ) {
        return MgmtDefaultParam(type);
    }
    if( length < 2U + 1U + value_length ) {
        return MgmtDefaultParam(type);
    }
    switch( value_length ) {
        case 2:
            return MgmtDefaultParam(type, jau::get_uint16(data + 2+1, jau::lb_endian_t::little));
        default:
            return MgmtDefaultParam(type);
    }
}

std::vector<MgmtDefaultParam> MgmtReadDefaultSysParamCmd::getParams(const uint8_t *data, const jau::nsize_t length) {
    std::vector<MgmtDefaultParam> res;
    jau::nsize_t consumed = 0;
    while( consumed < length && ( length - consumed ) > 2U + 1U ) {
        MgmtDefaultParam p = MgmtDefaultParam::read(data+consumed, length-consumed);
        if( !p.valid() ) {
            break;
        }
        consumed += p.mgmt_size();
        res.push_back( std::move(p) );
    }
    return res;
}

// *************************************************
// *************************************************
// *************************************************

std::unique_ptr<MgmtEvent> MgmtEvent::getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept {
    const MgmtEvent::Opcode opc = MgmtEvent::getOpcode(buffer);
    try {
        switch( opc ) {
            case MgmtEvent::Opcode::CMD_COMPLETE: {
                const MgmtCommand::Opcode cmdOpcode = MgmtEvtCmdComplete::getCmdOpcode(buffer);

                if( buffer_size >= MgmtEvtAdapterInfo::getRequiredTotalSize() &&
                    MgmtCommand::Opcode::READ_INFO == cmdOpcode ) {
                    return std::make_unique<MgmtEvtAdapterInfo>(buffer, buffer_size);
                } else if( buffer_size >= MgmtEvtPairDeviceComplete::getRequiredTotalSize() &&
                           MgmtCommand::Opcode::PAIR_DEVICE == cmdOpcode ) {
                    return std::make_unique<MgmtEvtPairDeviceComplete>(buffer, buffer_size);
                } else {
                    return std::make_unique<MgmtEvtCmdComplete>(buffer, buffer_size);
                }
            }
            case MgmtEvent::Opcode::CMD_STATUS:
                return std::make_unique<MgmtEvtCmdStatus>(buffer, buffer_size);
            case MgmtEvent::Opcode::CONTROLLER_ERROR:
                return std::make_unique<MgmtEvtControllerError>(buffer, buffer_size);
            case MgmtEvent::Opcode::INDEX_ADDED:
                return std::make_unique<MgmtEvent>(buffer, buffer_size, 0);
            case MgmtEvent::Opcode::INDEX_REMOVED:
                return std::make_unique<MgmtEvent>(buffer, buffer_size, 0);
            case MgmtEvent::Opcode::NEW_SETTINGS:
                return std::make_unique<MgmtEvtNewSettings>(buffer, buffer_size);
            case MgmtEvent::Opcode::LOCAL_NAME_CHANGED:
                return std::make_unique<MgmtEvtLocalNameChanged>(buffer, buffer_size);
            case Opcode::NEW_LINK_KEY:
                return std::make_unique<MgmtEvtNewLinkKey>(buffer, buffer_size);
            case Opcode::NEW_LONG_TERM_KEY:
                return std::make_unique<MgmtEvtNewLongTermKey>(buffer, buffer_size);
            case MgmtEvent::Opcode::DEVICE_CONNECTED:
                return std::make_unique<MgmtEvtDeviceConnected>(buffer, buffer_size);
            case MgmtEvent::Opcode::DEVICE_DISCONNECTED:
                return std::make_unique<MgmtEvtDeviceDisconnected>(buffer, buffer_size);
            case MgmtEvent::Opcode::CONNECT_FAILED:
                return std::make_unique<MgmtEvtDeviceConnectFailed>(buffer, buffer_size);
            case MgmtEvent::Opcode::PIN_CODE_REQUEST:
                return std::make_unique<MgmtEvtPinCodeRequest>(buffer, buffer_size);
            case MgmtEvent::Opcode::USER_CONFIRM_REQUEST:
                return std::make_unique<MgmtEvtUserConfirmRequest>(buffer, buffer_size);
            case MgmtEvent::Opcode::USER_PASSKEY_REQUEST:
                return std::make_unique<MgmtEvtUserPasskeyRequest>(buffer, buffer_size);
            case MgmtEvent::Opcode::PASSKEY_NOTIFY:
                return std::make_unique<MgmtEvtPasskeyNotify>(buffer, buffer_size);
            case Opcode::AUTH_FAILED:
                return std::make_unique<MgmtEvtAuthFailed>(buffer, buffer_size);
            case MgmtEvent::Opcode::DEVICE_FOUND:
                return std::make_unique<MgmtEvtDeviceFound>(buffer, buffer_size);
            case MgmtEvent::Opcode::DISCOVERING:
                return std::make_unique<MgmtEvtDiscovering>(buffer, buffer_size);
            case MgmtEvent::Opcode::DEVICE_UNPAIRED:
                return std::make_unique<MgmtEvtDeviceUnpaired>(buffer, buffer_size);
            case Opcode::NEW_IRK:
                return std::make_unique<MgmtEvtNewIdentityResolvingKey>(buffer, buffer_size);
            case Opcode::NEW_CSRK:
                return std::make_unique<MgmtEvtNewSignatureResolvingKey>(buffer, buffer_size);
            case MgmtEvent::Opcode::DEVICE_WHITELIST_ADDED:
                return std::make_unique<MgmtEvtDeviceWhitelistAdded>(buffer, buffer_size);
            case MgmtEvent::Opcode::DEVICE_WHITELIST_REMOVED:
                return std::make_unique<MgmtEvtDeviceWhitelistRemoved>(buffer, buffer_size);
            case MgmtEvent::Opcode::NEW_CONN_PARAM:
                return std::make_unique<MgmtEvtNewConnectionParam>(buffer, buffer_size);
            default:
                return std::make_unique<MgmtEvent>(buffer, buffer_size, 0);
        }
    } catch (...) {
        jau::fput_exception(stderr, std::current_exception(), E_FILE_LINE);
    }
    return nullptr;
}

// *************************************************
// *************************************************
// *************************************************

bool MgmtEvtCmdComplete::getCurrentSettings(AdapterSetting& current_settings) const noexcept {
    if( 4 != getDataSize() ) {
        return false;
    }
    MgmtCommand::Opcode cmd = getCmdOpcode();
    switch(cmd) {
        case MgmtCommand::Opcode::SET_POWERED:
            [[fallthrough]];
        case MgmtCommand::Opcode::SET_DISCOVERABLE:
            [[fallthrough]];
        case MgmtCommand::Opcode::SET_CONNECTABLE:
            [[fallthrough]];
        case MgmtCommand::Opcode::SET_FAST_CONNECTABLE:
            [[fallthrough]];
        case MgmtCommand::Opcode::SET_BONDABLE:
            [[fallthrough]];
        case MgmtCommand::Opcode::SET_LINK_SECURITY:
            [[fallthrough]];
        case MgmtCommand::Opcode::SET_SSP:
            [[fallthrough]];
        case MgmtCommand::Opcode::SET_HS:
            [[fallthrough]];
        case MgmtCommand::Opcode::SET_LE:
            [[fallthrough]];
        case MgmtCommand::Opcode::SET_ADVERTISING:
            [[fallthrough]];
        case MgmtCommand::Opcode::SET_BREDR:
            [[fallthrough]];
        case MgmtCommand::Opcode::SET_STATIC_ADDRESS:
            [[fallthrough]];
        case MgmtCommand::Opcode::SET_SECURE_CONN:
            [[fallthrough]];
        case MgmtCommand::Opcode::SET_DEBUG_KEYS:
            [[fallthrough]];
        case MgmtCommand::Opcode::SET_PRIVACY:
            current_settings = static_cast<AdapterSetting>( pdu.get_uint32_nc(getDataOffset()) );
            return true;
        default:
            return false;
    }
}

std::shared_ptr<ConnectionInfo> MgmtEvtCmdComplete::toConnectionInfo() const noexcept {
    if( MgmtCommand::Opcode::GET_CONN_INFO != getCmdOpcode() ) {
        jau_ERR_PRINT("Not a GET_CONN_INFO reply: %s", toString());
        return nullptr;
    }
    if( MgmtStatus::SUCCESS != getStatus() ) {
        jau_ERR_PRINT("No Success: %s", toString());
        return nullptr;
    }
    const jau::nsize_t min_size = ConnectionInfo::minimumDataSize();
    if( getDataSize() <  min_size ) {
        jau_ERR_PRINT("Data size < %zu: %s", min_size, toString());
        return nullptr;
    }

    const uint8_t *data = getData();
    if( nullptr == data ) {
        // Prelim checking to avoid g++ [8 - 10] giving a warning: '-Wnull-dereference' (impossible here!)
        jau_ERR_PRINT("Data nullptr: %s", toString());
        return nullptr;
    }
    EUI48 address = EUI48( data, jau::lb_endian_t::native );
    BDAddressType addressType = static_cast<BDAddressType>( jau::get_uint8(data + 6) );
    int8_t rssi = jau::get_int8(data + 7);
    int8_t tx_power = jau::get_int8(data + 8);
    int8_t max_tx_power = jau::get_int8(data + 9);
    return std::make_shared<ConnectionInfo>(address, addressType, rssi, tx_power, max_tx_power);
}

std::shared_ptr<NameAndShortName> MgmtEvtCmdComplete::toNameAndShortName() const noexcept {
    if( MgmtCommand::Opcode::SET_LOCAL_NAME != getCmdOpcode() ) {
        jau_ERR_PRINT("Not a SET_LOCAL_NAME reply: %s", toString());
        return nullptr;
    }
    if( MgmtStatus::SUCCESS != getStatus() ) {
        jau_ERR_PRINT("No Success: %s", toString());
        return nullptr;
    }
    const jau::nsize_t min_size = MgmtEvtLocalNameChanged::namesDataSize();
    if( getDataSize() <  min_size ) {
        jau_ERR_PRINT("Data size < %zu: %s", min_size, toString());
        return nullptr;
    }


    const uint8_t *data = getData();
    std::string name = std::string( (const char*) ( data ) );
    std::string short_name = std::string( (const char*) ( data + MGMT_MAX_NAME_LENGTH ) );

    return std::make_shared<NameAndShortName>(name, short_name);
}

std::shared_ptr<NameAndShortName> MgmtEvtLocalNameChanged::toNameAndShortName() const noexcept {
    return std::make_shared<NameAndShortName>(getName(), getShortName());
}

std::unique_ptr<AdapterInfo> MgmtEvtAdapterInfo::toAdapterInfo() const {
    return std::make_unique<AdapterInfo>(
            getDevID(),
            BDAddressAndType(getAddress(), BDAddressType::BDADDR_LE_PUBLIC),
            getVersion(),
            getManufacturer(), getSupportedSetting(),
            getCurrentSetting(), getDevClass(),
            getName(), getShortName());
}

bool MgmtEvtAdapterInfo::updateAdapterInfo(AdapterInfo& info) const noexcept {
    if( info.dev_id != getDevID() || info.addressAndType.address != getAddress() ) {
        return false;
    }
    info.setSettingMasks(getSupportedSetting(), getCurrentSetting());
    info.setDevClass(getDevClass());
    info.setName(getName());
    info.setShortName(getShortName());
    return true;
}

MgmtEvtDeviceDisconnected::DisconnectReason MgmtEvtDeviceDisconnected::getDisconnectReason(HCIStatusCode hciReason) noexcept {
    switch (hciReason) {
        case HCIStatusCode::CONNECTION_TIMEOUT:
            return DisconnectReason::TIMEOUT;
        case HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION:
        case HCIStatusCode::REMOTE_DEVICE_TERMINATED_CONNECTION_LOW_RESOURCES:
        case HCIStatusCode::REMOTE_DEVICE_TERMINATED_CONNECTION_POWER_OFF:
            return DisconnectReason::REMOTE;
        case HCIStatusCode::CONNECTION_TERMINATED_BY_LOCAL_HOST:
            return DisconnectReason::LOCAL_HOST;
        case HCIStatusCode::AUTHENTICATION_FAILURE:
            return DisconnectReason::AUTH_FAILURE;

        case HCIStatusCode::INTERNAL_FAILURE:
        case HCIStatusCode::UNKNOWN:
        default:
            return DisconnectReason::UNKNOWN;

    }
}
HCIStatusCode MgmtEvtDeviceDisconnected::getHCIReason(DisconnectReason mgmtReason) noexcept {
    switch(mgmtReason) {
        case DisconnectReason::TIMEOUT: return HCIStatusCode::CONNECTION_TIMEOUT;
        case DisconnectReason::LOCAL_HOST: return HCIStatusCode::CONNECTION_TERMINATED_BY_LOCAL_HOST;
        case DisconnectReason::REMOTE: return HCIStatusCode::REMOTE_USER_TERMINATED_CONNECTION;
        case DisconnectReason::AUTH_FAILURE: return HCIStatusCode::AUTHENTICATION_FAILURE;

        case DisconnectReason::UNKNOWN:
        default:
            return HCIStatusCode::UNKNOWN;
    }
}

HCIStatusCode direct_bt::to_HCIStatusCode(const MgmtStatus mstatus) noexcept {
    switch(mstatus) {
        case MgmtStatus::SUCCESS:           return HCIStatusCode::SUCCESS;
        case MgmtStatus::UNKNOWN_COMMAND:   return HCIStatusCode::UNKNOWN_COMMAND;
        case MgmtStatus::NOT_CONNECTED:     return HCIStatusCode::UNKNOWN_CONNECTION_IDENTIFIER;
        case MgmtStatus::FAILED:            return HCIStatusCode::FAILED;
        case MgmtStatus::CONNECT_FAILED:    return HCIStatusCode::CONNECT_FAILED;
        case MgmtStatus::AUTH_FAILED:       return HCIStatusCode::AUTH_FAILED;
        case MgmtStatus::NOT_PAIRED:        return HCIStatusCode::NOT_PAIRED;
        case MgmtStatus::NO_RESOURCES:      return HCIStatusCode::NO_RESOURCES;
        case MgmtStatus::TIMEOUT:           return HCIStatusCode::TIMEOUT;
        case MgmtStatus::ALREADY_CONNECTED: return HCIStatusCode::ALREADY_CONNECTED;
        case MgmtStatus::BUSY:              return HCIStatusCode::BUSY;
        case MgmtStatus::REJECTED:          return HCIStatusCode::REJECTED;
        case MgmtStatus::NOT_SUPPORTED:     return HCIStatusCode::NOT_SUPPORTED;
        case MgmtStatus::INVALID_PARAMS:    return HCIStatusCode::INVALID_PARAMS;
        case MgmtStatus::DISCONNECTED:      return HCIStatusCode::DISCONNECTED;
        case MgmtStatus::NOT_POWERED:       return HCIStatusCode::NOT_POWERED;
        case MgmtStatus::CANCELLED:         return HCIStatusCode::CANCELLED;
        case MgmtStatus::INVALID_INDEX:     return HCIStatusCode::INVALID_INDEX;
        case MgmtStatus::RFKILLED:          return HCIStatusCode::RFKILLED;
        case MgmtStatus::ALREADY_PAIRED:    return HCIStatusCode::ALREADY_PAIRED;
        case MgmtStatus::PERMISSION_DENIED: return HCIStatusCode::PERMISSION_DENIED;
        default:
            return HCIStatusCode::UNKNOWN;
    }
}

