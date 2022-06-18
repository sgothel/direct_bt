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

// #define PERF_PRINT_ON 1
#include <jau/debug.hpp>

#include "BTIoctl.hpp"

#include "MgmtTypes.hpp"
#include "HCIIoctl.hpp"
#include "HCIComm.hpp"
#include "BTTypes1.hpp"

extern "C" {
    #include <inttypes.h>
    #include <unistd.h>
}

using namespace direct_bt;

// *************************************************
// *************************************************
// *************************************************

#define CASE_TO_STRING(V) case V: return #V;
#define CASE2_TO_STRING(U,V) case U::V: return #V;

#define MGMT_STATUS_ENUM(X) \
    X(SUCCESS) \
    X(UNKNOWN_COMMAND) \
    X(NOT_CONNECTED) \
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
    X(PERMISSION_DENIED)

#define MGMT_STATUS_CASE_TO_STRING(V) case MgmtStatus::V: return #V;

std::string direct_bt::to_string(const MgmtStatus opc) noexcept {
    switch(opc) {
        MGMT_STATUS_ENUM(MGMT_STATUS_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown Status";
}

#define MGMT_LINKKEYTYPE_ENUM(X) \
    X(COMBI) \
    X(LOCAL_UNIT) \
    X(REMOTE_UNIT) \
    X(DBG_COMBI) \
    X(UNAUTH_COMBI_P192) \
    X(AUTH_COMBI_P192) \
    X(CHANGED_COMBI) \
    X(UNAUTH_COMBI_P256) \
    X(AUTH_COMBI_P256) \
    X(NONE)

#define MGMT_LINKKEYTYPE_TO_STRING(V) case MgmtLinkKeyType::V: return #V;

std::string direct_bt::to_string(const MgmtLinkKeyType type) noexcept {
    switch(type) {
        MGMT_LINKKEYTYPE_ENUM(MGMT_LINKKEYTYPE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown MgmtLinkKeyType";
}

#define MGMT_LTKTYPE_ENUM(X) \
    X(UNAUTHENTICATED) \
    X(AUTHENTICATED) \
    X(UNAUTHENTICATED_P256) \
    X(AUTHENTICATED_P256) \
    X(DEBUG_P256) \
    X(NONE)

#define MGMT_LTKTYPE_TO_STRING(V) case MgmtLTKType::V: return #V;

std::string direct_bt::to_string(const MgmtLTKType type) noexcept {
    switch(type) {
        MGMT_LTKTYPE_ENUM(MGMT_LTKTYPE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown MgmtLTKType";
}

MgmtLTKType direct_bt::to_MgmtLTKType(const SMPLongTermKey::Property mask) noexcept {
    if( ( SMPLongTermKey::Property::AUTH & mask ) != SMPLongTermKey::Property::NONE ) {
        return ( SMPLongTermKey::Property::SC & mask ) != SMPLongTermKey::Property::NONE ?
                MgmtLTKType::AUTHENTICATED_P256 : MgmtLTKType::AUTHENTICATED;
    } else {
        return ( SMPLongTermKey::Property::SC & mask ) != SMPLongTermKey::Property::NONE ?
                MgmtLTKType::UNAUTHENTICATED_P256 : MgmtLTKType::UNAUTHENTICATED;
    }
}

#define MGMT_CSRKTYPE_ENUM(X) \
    X(UNAUTHENTICATED_LOCAL) \
    X(UNAUTHENTICATED_REMOTE) \
    X(AUTHENTICATED_LOCAL) \
    X(AUTHENTICATED_REMOTE) \
    X(NONE)

#define MGMT_CSRKTYPE_TO_STRING(V) case MgmtCSRKType::V: return #V;

std::string direct_bt::to_string(const MgmtCSRKType type) noexcept {
    switch(type) {
        MGMT_CSRKTYPE_ENUM(MGMT_CSRKTYPE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown MgmtCSRKType";
}


// *************************************************
// *************************************************
// *************************************************

#define MGMT_OPCODE_ENUM(X) \
    X(READ_VERSION) \
    X(READ_COMMANDS) \
    X(READ_INDEX_LIST) \
    X(READ_INFO) \
    X(SET_POWERED) \
    X(SET_DISCOVERABLE) \
    X(SET_CONNECTABLE) \
    X(SET_FAST_CONNECTABLE) \
    X(SET_BONDABLE) \
    X(SET_LINK_SECURITY) \
    X(SET_SSP) \
    X(SET_HS) \
    X(SET_LE) \
    X(SET_DEV_CLASS) \
    X(SET_LOCAL_NAME) \
    X(ADD_UUID) \
    X(REMOVE_UUID) \
    X(LOAD_LINK_KEYS) \
    X(LOAD_LONG_TERM_KEYS) \
    X(DISCONNECT) \
    X(GET_CONNECTIONS) \
    X(PIN_CODE_REPLY) \
    X(PIN_CODE_NEG_REPLY) \
    X(SET_IO_CAPABILITY) \
    X(PAIR_DEVICE) \
    X(CANCEL_PAIR_DEVICE) \
    X(UNPAIR_DEVICE) \
    X(USER_CONFIRM_REPLY) \
    X(USER_CONFIRM_NEG_REPLY) \
    X(USER_PASSKEY_REPLY) \
    X(USER_PASSKEY_NEG_REPLY) \
    X(READ_LOCAL_OOB_DATA) \
    X(ADD_REMOTE_OOB_DATA) \
    X(REMOVE_REMOTE_OOB_DATA) \
    X(START_DISCOVERY) \
    X(STOP_DISCOVERY) \
    X(CONFIRM_NAME) \
    X(BLOCK_DEVICE) \
    X(UNBLOCK_DEVICE) \
    X(SET_DEVICE_ID) \
    X(SET_ADVERTISING) \
    X(SET_BREDR) \
    X(SET_STATIC_ADDRESS) \
    X(SET_SCAN_PARAMS) \
    X(SET_SECURE_CONN) \
    X(SET_DEBUG_KEYS) \
    X(SET_PRIVACY) \
    X(LOAD_IRKS) \
    X(GET_CONN_INFO) \
    X(GET_CLOCK_INFO) \
    X(ADD_DEVICE_WHITELIST) \
    X(REMOVE_DEVICE_WHITELIST) \
    X(LOAD_CONN_PARAM) \
    X(READ_UNCONF_INDEX_LIST) \
    X(READ_CONFIG_INFO) \
    X(SET_EXTERNAL_CONFIG) \
    X(SET_PUBLIC_ADDRESS) \
    X(START_SERVICE_DISCOVERY) \
    X(READ_LOCAL_OOB_EXT_DATA) \
    X(READ_EXT_INDEX_LIST) \
    X(READ_ADV_FEATURES) \
    X(ADD_ADVERTISING) \
    X(REMOVE_ADVERTISING) \
    X(GET_ADV_SIZE_INFO) \
    X(START_LIMITED_DISCOVERY) \
    X(READ_EXT_INFO) \
    X(SET_APPEARANCE) \
    X(GET_PHY_CONFIGURATION) \
    X(SET_PHY_CONFIGURATION) \
    X(SET_BLOCKED_KEYS) \
    X(SET_WIDEBAND_SPEECH) \
    X(READ_SECURITY_INFO) \
    X(READ_EXP_FEATURES_INFO) \
    X(SET_EXP_FEATURE) \
    X(READ_DEF_SYSTEM_CONFIG) \
    X(SET_DEF_SYSTEM_CONFIG) \
    X(READ_DEF_RUNTIME_CONFIG) \
    X(SET_DEF_RUNTIME_CONFIG) \
    X(GET_DEVICE_FLAGS) \
    X(SET_DEVICE_FLAGS) \
    X(READ_ADV_MONITOR_FEATURES) \
    X(ADD_ADV_PATTERNS_MONITOR) \
    X(REMOVE_ADV_MONITOR)

#define MGMT_OPCODE_CASE_TO_STRING(V) case MgmtCommand::Opcode::V: return #V;

std::string MgmtCommand::getOpcodeString(const Opcode op) noexcept {
    switch(op) {
        MGMT_OPCODE_ENUM(MGMT_OPCODE_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown Operation";
}

// *************************************************
// *************************************************
// *************************************************

#define MGMT_DEFPARAMTYPE_ENUM(X) \
    X(BREDR_PAGE_SCAN_TYPE) \
    X(BREDR_PAGE_SCAN_INTERVAL) \
    X(BREDR_PAGE_SCAN_WINDOW) \
    X(BREDR_INQUIRY_TYPE) \
    X(BREDR_INQUIRY_INTERVAL) \
    X(BREDR_INQUIRY_WINDOW) \
    X(BREDR_LINK_SUPERVISOR_TIMEOUT) \
    X(BREDR_PAGE_TIMEOUT) \
    X(BREDR_MIN_SNIFF_INTERVAL) \
    X(BREDR_MAX_SNIFF_INTERVAL) \
    X(LE_ADV_MIN_INTERVAL) \
    X(LE_ADV_MAX_INTERVAL) \
    X(LE_MULTI_ADV_ROT_INTERVAL) \
    X(LE_SCAN_INTERVAL_AUTOCONN) \
    X(LE_SCAN_WINDOW_AUTOCONN) \
    X(LE_SCAN_INTERVAL_WAKESCENARIO) \
    X(LE_SCAN_WINDOW_WAKESCENARIO) \
    X(LE_SCAN_INTERVAL_DISCOVERY) \
    X(LE_SCAN_WINDOW_DISCOVERY) \
    X(LE_SCAN_INTERVAL_ADVMON) \
    X(LE_SCAN_WINDOW_ADVMON) \
    X(LE_SCAN_INTERVAL_CONNECT) \
    X(LE_SCAN_WINDOW_CONNECT) \
    X(LE_MIN_CONN_INTERVAL) \
    X(LE_MAX_CONN_INTERVAL) \
    X(LE_CONN_LATENCY) \
    X(LE_CONN_SUPERVISOR_TIMEOUT) \
    X(LE_AUTOCONN_TIMEOUT) \
    X(NONE)

#define MGMT_DEFPARAMTYPE_CASE_TO_STRING(V) case MgmtDefaultParam::Type::V: return #V;

std::string MgmtDefaultParam::getTypeString(const Type op) noexcept {
    switch(op) {
        MGMT_DEFPARAMTYPE_ENUM(MGMT_DEFPARAMTYPE_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown Type";
}

MgmtDefaultParam MgmtDefaultParam::read(const uint8_t* data, const jau::nsize_t length) noexcept {
    if( length < 2U ) {
        return MgmtDefaultParam();
    }
    const Type type = static_cast<Type>( jau::get_uint16(data, 0, true /* little_endian */) );
    if( length < 2U + 1U ) {
        return MgmtDefaultParam(type);
    }
    const uint8_t value_length = jau::get_uint8(data, 2);
    if( value_length != to_size(type) ) {
        return MgmtDefaultParam(type);
    }
    if( length < 2U + 1U + value_length ) {
        return MgmtDefaultParam(type);
    }
    switch( value_length ) {
        case 2:
            return MgmtDefaultParam(type, jau::get_uint16(data, 2+1, true /* little */));
        default:
            return MgmtDefaultParam(type);
    }
}

std::vector<MgmtDefaultParam> MgmtReadDefaultSysParamCmd::getParams(const uint8_t *data, const jau::nsize_t length) noexcept {
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

#define MGMT_EV_OPCODE_ENUM(X) \
    X(INVALID) \
    X(CMD_COMPLETE) \
    X(CMD_STATUS) \
    X(CONTROLLER_ERROR) \
    X(INDEX_ADDED) \
    X(INDEX_REMOVED) \
    X(NEW_SETTINGS) \
    X(CLASS_OF_DEV_CHANGED) \
    X(LOCAL_NAME_CHANGED) \
    X(NEW_LINK_KEY) \
    X(NEW_LONG_TERM_KEY) \
    X(DEVICE_CONNECTED) \
    X(DEVICE_DISCONNECTED) \
    X(CONNECT_FAILED) \
    X(PIN_CODE_REQUEST) \
    X(USER_CONFIRM_REQUEST) \
    X(USER_PASSKEY_REQUEST) \
    X(AUTH_FAILED) \
    X(DEVICE_FOUND) \
    X(DISCOVERING) \
    X(DEVICE_BLOCKED) \
    X(DEVICE_UNBLOCKED) \
    X(DEVICE_UNPAIRED) \
    X(PASSKEY_NOTIFY) \
    X(NEW_IRK) \
    X(NEW_CSRK) \
    X(DEVICE_WHITELIST_ADDED) \
    X(DEVICE_WHITELIST_REMOVED) \
    X(NEW_CONN_PARAM) \
    X(UNCONF_INDEX_ADDED) \
    X(UNCONF_INDEX_REMOVED) \
    X(NEW_CONFIG_OPTIONS) \
    X(EXT_INDEX_ADDED) \
    X(EXT_INDEX_REMOVED) \
    X(LOCAL_OOB_DATA_UPDATED) \
    X(ADVERTISING_ADDED) \
    X(ADVERTISING_REMOVED) \
    X(EXT_INFO_CHANGED) \
    X(PHY_CONFIGURATION_CHANGED) \
    X(EXP_FEATURE_CHANGED) \
    X(DEVICE_FLAGS_CHANGED) \
    X(ADV_MONITOR_ADDED) \
    X(ADV_MONITOR_REMOVED) \
    X(PAIR_DEVICE_COMPLETE) \
    X(HCI_ENC_CHANGED) \
    X(HCI_ENC_KEY_REFRESH_COMPLETE) \
    X(HCI_LE_REMOTE_FEATURES) \
    X(HCI_LE_PHY_UPDATE_COMPLETE) \
    X(HCI_LE_LTK_REQUEST) \
    X(HCI_LE_LTK_REPLY_ACK) \
    X(HCI_LE_LTK_REPLY_REJ) \
    X(HCI_LE_ENABLE_ENC)

#define MGMT_EV_OPCODE_CASE_TO_STRING(V) case MgmtEvent::Opcode::V: return #V;

std::string MgmtEvent::getOpcodeString(const Opcode opc) noexcept {
    switch(opc) {
        MGMT_EV_OPCODE_ENUM(MGMT_EV_OPCODE_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown Opcode";
}

std::unique_ptr<MgmtEvent> MgmtEvent::getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept {
    const MgmtEvent::Opcode opc = MgmtEvent::getOpcode(buffer);
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
        ERR_PRINT("Not a GET_CONN_INFO reply: %s", toString().c_str());
        return nullptr;
    }
    if( MgmtStatus::SUCCESS != getStatus() ) {
        ERR_PRINT("No Success: %s", toString().c_str());
        return nullptr;
    }
    const jau::nsize_t min_size = ConnectionInfo::minimumDataSize();
    if( getDataSize() <  min_size ) {
        ERR_PRINT("Data size < %d: %s", min_size, toString().c_str());
        return nullptr;
    }

    const uint8_t *data = getData();
    if( nullptr == data ) {
        // Prelim checking to avoid g++ [8 - 10] giving a warning: '-Wnull-dereference' (impossible here!)
        ERR_PRINT("Data nullptr: %s", toString().c_str());
        return nullptr;
    }
    EUI48 address = EUI48( data, jau::endian::native );
    BDAddressType addressType = static_cast<BDAddressType>( jau::get_uint8(data, 6) );
    int8_t rssi = jau::get_int8(data, 7);
    int8_t tx_power = jau::get_int8(data, 8);
    int8_t max_tx_power = jau::get_int8(data, 9);
    return std::make_shared<ConnectionInfo>(address, addressType, rssi, tx_power, max_tx_power);
}

std::shared_ptr<NameAndShortName> MgmtEvtCmdComplete::toNameAndShortName() const noexcept {
    if( MgmtCommand::Opcode::SET_LOCAL_NAME != getCmdOpcode() ) {
        ERR_PRINT("Not a SET_LOCAL_NAME reply: %s", toString().c_str());
        return nullptr;
    }
    if( MgmtStatus::SUCCESS != getStatus() ) {
        ERR_PRINT("No Success: %s", toString().c_str());
        return nullptr;
    }
    const jau::nsize_t min_size = MgmtEvtLocalNameChanged::namesDataSize();
    if( getDataSize() <  min_size ) {
        ERR_PRINT("Data size < %d: %s", min_size, toString().c_str());
        return nullptr;
    }


    const uint8_t *data = getData();
    std::string name = std::string( (const char*) ( data ) );
    std::string short_name = std::string( (const char*) ( data + MgmtConstU16::MGMT_MAX_NAME_LENGTH ) );

    return std::make_shared<NameAndShortName>(name, short_name);
}

std::shared_ptr<NameAndShortName> MgmtEvtLocalNameChanged::toNameAndShortName() const noexcept {
    return std::make_shared<NameAndShortName>(getName(), getShortName());
}

std::unique_ptr<AdapterInfo> MgmtEvtAdapterInfo::toAdapterInfo() const noexcept {
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

std::string MgmtEvtDeviceDisconnected::getDisconnectReasonString(DisconnectReason mgmtReason) noexcept {
    switch(mgmtReason) {
        case DisconnectReason::TIMEOUT: return "TIMEOUT";
        case DisconnectReason::LOCAL_HOST: return "LOCAL_HOST";
        case DisconnectReason::REMOTE: return "REMOTE";
        case DisconnectReason::AUTH_FAILURE: return "AUTH_FAILURE";

        case DisconnectReason::UNKNOWN:
        default:
            return "UNKNOWN";
    }
    return "Unknown DisconnectReason";
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
