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
#include "DBTTypes.hpp"

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

std::string direct_bt::getMgmtStatusString(const MgmtStatus opc) noexcept {
    switch(opc) {
        MGMT_STATUS_ENUM(MGMT_STATUS_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown Status";
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
    X(HCI_ENC_CHANGED) \
    X(HCI_ENC_KEY_REFRESH_COMPLETE) \
    X(HCI_LE_REMOTE_USR_FEATURES)

#define MGMT_EV_OPCODE_CASE_TO_STRING(V) case MgmtEvent::Opcode::V: return #V;

std::string MgmtEvent::getOpcodeString(const Opcode opc) noexcept {
    switch(opc) {
        MGMT_EV_OPCODE_ENUM(MGMT_EV_OPCODE_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown Opcode";
}

std::shared_ptr<MgmtEvent> MgmtEvent::getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept {
    const MgmtEvent::Opcode opc = static_cast<MgmtEvent::Opcode>( jau::get_uint16(buffer, 0, true /* littleEndian */) );
    MgmtEvent * res;
    switch( opc ) {
        case MgmtEvent::Opcode::CMD_COMPLETE:
            if( buffer_size >= MgmtEvtAdapterInfo::getRequiredTotalSize() &&
                MgmtCommand::Opcode::READ_INFO == MgmtEvtCmdComplete::getCmdOpcode(buffer) ) {
                res = new MgmtEvtAdapterInfo(buffer, buffer_size);
            } else {
                res = new MgmtEvtCmdComplete(buffer, buffer_size);
            }
            break;
        case MgmtEvent::Opcode::CMD_STATUS:
            res = new MgmtEvtCmdStatus(buffer, buffer_size); break;
        case MgmtEvent::Opcode::CONTROLLER_ERROR:
            res = new MgmtEvtControllerError(buffer, buffer_size); break;
        case MgmtEvent::Opcode::INDEX_ADDED:
            res = new MgmtEvent(buffer, buffer_size, 0); break;
        case MgmtEvent::Opcode::INDEX_REMOVED:
            res = new MgmtEvent(buffer, buffer_size, 0); break;
        case MgmtEvent::Opcode::NEW_SETTINGS:
            res = new MgmtEvtNewSettings(buffer, buffer_size); break;
        case MgmtEvent::Opcode::LOCAL_NAME_CHANGED:
            res = new MgmtEvtLocalNameChanged(buffer, buffer_size); break;
        case Opcode::NEW_LINK_KEY:
            res = new MgmtEvtNewLinkKey(buffer, buffer_size); break;
        case Opcode::NEW_LONG_TERM_KEY:
            res = new MgmtEvtNewLongTermKey(buffer, buffer_size); break;
        case MgmtEvent::Opcode::DEVICE_CONNECTED:
            res = new MgmtEvtDeviceConnected(buffer, buffer_size); break;
        case MgmtEvent::Opcode::DEVICE_DISCONNECTED:
            res = new MgmtEvtDeviceDisconnected(buffer, buffer_size); break;
        case MgmtEvent::Opcode::CONNECT_FAILED:
            res = new MgmtEvtDeviceConnectFailed(buffer, buffer_size); break;
        case MgmtEvent::Opcode::PIN_CODE_REQUEST:
            res = new MgmtEvtPinCodeRequest(buffer, buffer_size); break;
        case MgmtEvent::Opcode::USER_CONFIRM_REQUEST:
            res = new MgmtEvtUserConfirmRequest(buffer, buffer_size); break;
        case MgmtEvent::Opcode::USER_PASSKEY_REQUEST:
            res = new MgmtEvtUserPasskeyRequest(buffer, buffer_size); break;
        case Opcode::AUTH_FAILED:
            res = new MgmtEvtAuthFailed(buffer, buffer_size); break;
        case MgmtEvent::Opcode::DEVICE_FOUND:
            res = new MgmtEvtDeviceFound(buffer, buffer_size); break;
        case MgmtEvent::Opcode::DISCOVERING:
            res = new MgmtEvtDiscovering(buffer, buffer_size); break;
        case MgmtEvent::Opcode::DEVICE_UNPAIRED:
            res = new MgmtEvtDeviceUnpaired(buffer, buffer_size); break;
        case Opcode::NEW_IRK:
            res = new MgmtEvtNewIdentityResolvingKey(buffer, buffer_size); break;
        case MgmtEvent::Opcode::DEVICE_WHITELIST_ADDED:
            res = new MgmtEvtDeviceWhitelistAdded(buffer, buffer_size); break;
        case MgmtEvent::Opcode::DEVICE_WHITELIST_REMOVED:
            res = new MgmtEvtDeviceWhitelistRemoved(buffer, buffer_size); break;
        case MgmtEvent::Opcode::NEW_CONN_PARAM:
            res = new MgmtEvtNewConnectionParam(buffer, buffer_size); break;
        default:
            res = new MgmtEvent(buffer, buffer_size, 0); break;
    }
    return std::shared_ptr<MgmtEvent>( res );
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
    EUI48 address = EUI48( data );
    BDAddressType addressType = static_cast<BDAddressType>( jau::get_uint8(data, 6) );
    int8_t rssi = jau::get_int8(data, 7);
    int8_t tx_power = jau::get_int8(data, 8);
    int8_t max_tx_power = jau::get_int8(data, 9);
    return std::shared_ptr<ConnectionInfo>(new ConnectionInfo(address, addressType, rssi, tx_power, max_tx_power) );
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

    return std::shared_ptr<NameAndShortName>(new NameAndShortName(name, short_name) );
}

std::shared_ptr<NameAndShortName> MgmtEvtLocalNameChanged::toNameAndShortName() const noexcept {
    return std::shared_ptr<NameAndShortName>(new NameAndShortName(getName(), getShortName()) );
}

std::shared_ptr<AdapterInfo> MgmtEvtAdapterInfo::toAdapterInfo() const noexcept {
    return std::shared_ptr<AdapterInfo>(new AdapterInfo(
            getDevID(), getAddress(), getVersion(),
            getManufacturer(), getSupportedSetting(),
            getCurrentSetting(), getDevClass(),
            getName(), getShortName()) );
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

HCIStatusCode direct_bt::getHCIStatusCode(const MgmtStatus mstatus) noexcept {
    switch(mstatus) {
        case MgmtStatus::SUCCESS:           return HCIStatusCode::SUCCESS;
        case MgmtStatus::UNKNOWN_COMMAND:   return HCIStatusCode::UNKNOWN_HCI_COMMAND;
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
