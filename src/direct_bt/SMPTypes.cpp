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

#include <jau/debug.hpp>

#include "SMPTypes.hpp"

using namespace direct_bt;

#define AUTHREQ_ENUM(X) \
    X(NONE) \
    X(BONDING) \
    X(BONDING_RESERVED) \
    X(MITM) \
    X(LE_SECURE_CONNECTIONS) \
    X(KEYPRESS) \
    X(CT2_H7_FUNC_SUPPORT) \
    X(RFU_1) \
    X(RFU_2)

#define CASE_TO_STRING0(V) case SMPAuthReqs::V: return #V;

std::string direct_bt::getSMPAuthReqBitString(const SMPAuthReqs bit) noexcept {
    switch(bit) {
        AUTHREQ_ENUM(CASE_TO_STRING0)
        default: ; // fall through intended
    }
    return "Unknown AuthRequirements bit";
}

std::string direct_bt::getSMPAuthReqMaskString(const SMPAuthReqs mask) noexcept {
    const uint8_t one = 1;
    bool has_pre = false;
    std::string out("[");
    for(int i=0; i<8; i++) {
        const uint8_t settingBit = one << i;
        if( 0 != ( static_cast<uint8_t>(mask) & settingBit ) ) {
            if( has_pre ) { out.append(", "); }
            out.append( getSMPAuthReqBitString( static_cast<SMPAuthReqs>(settingBit) ) );
            has_pre = true;
        }
    }
    out.append("]");
    return out;
}


#define OPCODE_ENUM(X) \
        X(UNDEFINED) \
        X(PAIRING_REQUEST) \
        X(PAIRING_RESPONSE) \
        X(PAIRING_CONFIRM) \
        X(PAIRING_RANDOM) \
        X(PAIRING_FAILED) \
        X(ENCRYPTION_INFORMATION) \
        X(MASTER_IDENTIFICATION) \
        X(IDENTITY_INFORMATION) \
        X(IDENTITY_ADDRESS_INFORMATION) \
        X(SIGNING_INFORMATION) \
        X(SECURITY_REQUEST) \
        X(PAIRING_PUBLIC_KEY) \
        X(PAIRING_DHKEY_CHECK) \
        X(PAIRING_KEYPRESS_NOTIFICATION)

#define CASE_TO_STRING1(V) case Opcode::V: return #V;

std::string SMPPDUMsg::getOpcodeString(const Opcode opc) noexcept {
    switch(opc) {
        OPCODE_ENUM(CASE_TO_STRING1)
        default: ; // fall through intended
    }
    return "Unknown Opcode";
}

std::string SMPPairFailedMsg::getPlainReasonString(const ReasonCode reasonCode) noexcept {
    switch(reasonCode) {
        case ReasonCode::UNDEFINED: return "Undefined";
        case ReasonCode::PASSKEY_ENTRY_FAILED: return "Passkey Entry Failed";
        case ReasonCode::OOB_NOT_AVAILABLE: return "OOB Not Available";
        case ReasonCode::AUTHENTICATION_REQUIREMENTS: return "Authentication Requirements";
        case ReasonCode::CONFIRM_VALUE_FAILED: return "Confirm Value Failed";
        case ReasonCode::PAIRING_NOT_SUPPORTED: return "Pairing Not Supported";
        case ReasonCode::ENCRYPTION_KEY_SIZE: return "Encryption Key Size";
        case ReasonCode::COMMON_NOT_SUPPORTED: return "Common Not Supported";
        case ReasonCode::UNSPECIFIED_REASON: return "Unspecified Reason";
        case ReasonCode::REPEATED_ATTEMPTS: return "Repeated Attempts";
        case ReasonCode::INVALID_PARAMTERS: return "Invalid Paramters";
        case ReasonCode::DHKEY_CHECK_FAILED: return "DHKey Check Failed";
        case ReasonCode::NUMERIC_COMPARISON_FAILED: return "Numeric Comparison Failed";
        case ReasonCode::BREDR_PAIRING_IN_PROGRESS: return "BR/EDR pairing in process";
        case ReasonCode::CROSSXPORT_KEY_DERIGEN_NOT_ALLOWED: return "Cross-transport Key Derivation/Generation not allowed";
        default: ; // fall through intended
    }
    return "Reason reserved for future use";
}

#define IOCAP_ENUM(X) \
        X(DISPLAY_ONLY) \
        X(DISPLAY_YES_NO) \
        X(KEYBOARD_ONLY) \
        X(NO_INPUT_NO_OUTPUT) \
        X(KEYBOARD_DISPLAY)

#define CASE_TO_STRING2(V) case IOCapability::V: return #V;

std::string SMPPairingMsg::getIOCapabilityString(const IOCapability ioc) noexcept {
    switch(ioc) {
        IOCAP_ENUM(CASE_TO_STRING2)
        default: ; // fall through intended
    }
    return "Unknown IOCapability";
}

std::string SMPPairingMsg::getOOBDataFlagString(const OOBDataFlag v) noexcept {
    switch(v) {
        case OOBDataFlag::OOB_AUTH_DATA_NOT_PRESENT: return "OOB_AUTH_DATA_NOT_PRESENT";
        case OOBDataFlag::OOB_AUTH_DATA_REMOTE_PRESENT: return "OOB_AUTH_DATA_REMOTE_PRESENT";
        default: ; // fall through intended
    }
    return "Unknown OOBDataFlag";
}

#define KEYDISTFMT_ENUM(X) \
    X(NONE) \
    X(ENC_KEY) \
    X(ID_KEY) \
    X(SIGN_KEY) \
    X(LINK_KEY) \
    X(RFU_1) \
    X(RFU_2) \
    X(RFU_3) \
    X(RFU_4)

#define CASE_TO_STRING3(V) case KeyDistFormat::V: return #V;

std::string SMPPairingMsg::getKeyDistFormatBitString(const KeyDistFormat bit) noexcept {
    switch(bit) {
        KEYDISTFMT_ENUM(CASE_TO_STRING3)
        default: ; // fall through intended
    }
    return "Unknown KeyDistributionFormat bit";
}

std::string SMPPairingMsg::getKeyDistFormatMaskString(const KeyDistFormat mask) noexcept {
    const uint8_t one = 1;
    bool has_pre = false;
    std::string out("[");
    for(int i=0; i<8; i++) {
        const uint8_t settingBit = one << i;
        if( 0 != ( static_cast<uint8_t>(mask) & settingBit ) ) {
            if( has_pre ) { out.append(", "); }
            out.append( getKeyDistFormatBitString( static_cast<KeyDistFormat>(settingBit) ) );
            has_pre = true;
        }
    }
    out.append("]");
    return out;
}

#define TYPECODE_ENUM(X) \
    X(PASSKEY_ENTRY_STARTED) \
    X(PASSKEY_DIGIT_ENTERED) \
    X(PASSKEY_DIGIT_ERASED) \
    X(PASSKEY_CLEARED) \
    X(PASSKEY_ENTRY_COMPLETED)

#define CASE_TO_STRING4(V) case TypeCode::V: return #V;

std::string SMPPasskeyNotification::getTypeCodeString(const TypeCode tc) noexcept {
    switch(tc) {
        TYPECODE_ENUM(CASE_TO_STRING4)
        default: ; // fall through intended
    }
    return "Unknown TypeCode";
}

std::shared_ptr<const SMPPDUMsg> SMPPDUMsg::getSpecialized(const uint8_t * buffer, jau::nsize_t const buffer_size) noexcept {
    const SMPPDUMsg::Opcode opc = static_cast<SMPPDUMsg::Opcode>(*buffer);
    const SMPPDUMsg * res;
    switch( opc ) {
        case Opcode::PAIRING_REQUEST:               res = new SMPPairingMsg(true /* request */, buffer, buffer_size); break;
        case Opcode::PAIRING_RESPONSE:              res = new SMPPairingMsg(false /* request */, buffer, buffer_size); break;
        case Opcode::PAIRING_CONFIRM:               res = new SMPPairConfMsg(buffer, buffer_size); break;
        case Opcode::PAIRING_RANDOM:                res = new SMPPairRandMsg(buffer, buffer_size); break;
        case Opcode::PAIRING_FAILED:                res = new SMPPairFailedMsg(buffer, buffer_size); break;
        case Opcode::ENCRYPTION_INFORMATION:        res = new SMPEncInfoMsg(buffer, buffer_size); break;
        case Opcode::MASTER_IDENTIFICATION:         res = new SMPMasterIdentMsg(buffer, buffer_size); break;
        case Opcode::IDENTITY_INFORMATION:          res = new SMPIdentInfoMsg(buffer, buffer_size); break;
        case Opcode::IDENTITY_ADDRESS_INFORMATION:  res = new SMPIdentAddrInfoMsg(buffer, buffer_size); break;
        case Opcode::SIGNING_INFORMATION:           res = new SMPSignInfoMsg(buffer, buffer_size); break;
        case Opcode::SECURITY_REQUEST:              res = new SMPSecurityReqMsg(buffer, buffer_size); break;
        case Opcode::PAIRING_PUBLIC_KEY:            res = new SMPPairPubKeyMsg(buffer, buffer_size); break;
        case Opcode::PAIRING_DHKEY_CHECK:           res = new SMPPairDHKeyCheckMsg(buffer, buffer_size); break;
        case Opcode::PAIRING_KEYPRESS_NOTIFICATION: res = new SMPPasskeyNotification(buffer, buffer_size); break;
        default:                                    res = new SMPPDUMsg(buffer, buffer_size); break;
    }
    return std::shared_ptr<const SMPPDUMsg>(res);
}
