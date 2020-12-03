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

#define PAIRSTATE_ENUM(X) \
        X(NONE) \
        X(FAILED) \
        X(REQUESTED_BY_RESPONDER) \
        X(FEATURE_EXCHANGE_STARTED) \
        X(FEATURE_EXCHANGE_COMPLETED) \
        X(PASSKEY_EXPECTED) \
        X(NUMERIC_COMPARE_EXPECTED) \
        X(OOB_EXPECTED) \
        X(KEY_DISTRIBUTION) \
        X(COMPLETED)

#define CASE_TO_STRING_PAIRSTATE(V) case SMPPairingState::V: return #V;

std::string direct_bt::getSMPPairingStateString(const SMPPairingState state) noexcept {
    switch(state) {
        PAIRSTATE_ENUM(CASE_TO_STRING_PAIRSTATE)
        default: ; // fall through intended
    }
    return "Unknown SMP PairingState";
}

#define IOCAP_ENUM(X) \
        X(DISPLAY_ONLY) \
        X(DISPLAY_YES_NO) \
        X(KEYBOARD_ONLY) \
        X(NO_INPUT_NO_OUTPUT) \
        X(KEYBOARD_DISPLAY) \
        X(UNSET)

#define CASE_TO_STRING_IOCAP(V) case SMPIOCapability::V: return #V;

std::string direct_bt::getSMPIOCapabilityString(const SMPIOCapability ioc) noexcept {
    switch(ioc) {
        IOCAP_ENUM(CASE_TO_STRING_IOCAP)
        default: ; // fall through intended
    }
    return "Unknown SMP IOCapability";
}

std::string direct_bt::getSMPOOBDataFlagString(const SMPOOBDataFlag v) noexcept {
    switch(v) {
        case SMPOOBDataFlag::OOB_AUTH_DATA_NOT_PRESENT: return "OOB_AUTH_DATA_NOT_PRESENT";
        case SMPOOBDataFlag::OOB_AUTH_DATA_REMOTE_PRESENT: return "OOB_AUTH_DATA_REMOTE_PRESENT";
        default: ; // fall through intended
    }
    return "Unknown SMP OOBDataFlag";
}


std::string direct_bt::getSMPAuthReqBitString(const SMPAuthReqs bit) noexcept {
    switch(bit) {
        case SMPAuthReqs::NONE:                return "none";
        case SMPAuthReqs::BONDING:             return "Bonding";
        case SMPAuthReqs::BONDING_RFU:         return "Bonding_RFU";
        case SMPAuthReqs::MITM:                return "MITM";
        case SMPAuthReqs::SECURE_CONNECTIONS:  return "SC";
        case SMPAuthReqs::KEYPRESS:            return "Keypresses";
        case SMPAuthReqs::CT2_H7_FUNC_SUPPORT: return "CT2_H7";
        case SMPAuthReqs::RFU_1:               return "RFU_1";
        case SMPAuthReqs::RFU_2:               return "RFU_2";
        default: ; // fall through intended
    }
    return "Unknown AuthRequirements bit";
}

std::string direct_bt::getSMPAuthReqMaskString(const SMPAuthReqs mask) noexcept {
    std::string out("[");
    if( isSMPAuthReqBitSet(mask, SMPAuthReqs::BONDING) ) {
        out.append("Bonding");
    } else {
        out.append("No bonding");
    }
    if( isSMPAuthReqBitSet(mask, SMPAuthReqs::BONDING_RFU) ) {
        out.append(", ");
        out.append("Bonding Reserved");
    }
    out.append(", ");
    if( isSMPAuthReqBitSet(mask, SMPAuthReqs::MITM) ) {
        out.append("MITM");
    } else {
        out.append("No MITM");
    }
    out.append(", ");
    if( isSMPAuthReqBitSet(mask, SMPAuthReqs::SECURE_CONNECTIONS) ) {
        out.append("SC");
    } else {
        out.append("Legacy");
    }
    out.append(", ");
    if( isSMPAuthReqBitSet(mask, SMPAuthReqs::KEYPRESS) ) {
        out.append("Keypresses");
    } else {
        out.append("No keypresses");
    }
    if( isSMPAuthReqBitSet(mask, SMPAuthReqs::CT2_H7_FUNC_SUPPORT) ) {
        out.append(", ");
        out.append("CT2_H7");
    }
    if( isSMPAuthReqBitSet(mask, SMPAuthReqs::RFU_1) ) {
        out.append(", ");
        out.append("RFU_1");
    }
    if( isSMPAuthReqBitSet(mask, SMPAuthReqs::RFU_2) ) {
        out.append(", ");
        out.append("RFU_2");
    }
    out.append("]");
    return out;
}

PairingMode direct_bt::getPairingMode(const bool use_sc,
                                      const SMPAuthReqs authReqs_ini, const SMPIOCapability ioCap_ini, const SMPOOBDataFlag oobFlag_ini,
                                      const SMPAuthReqs authReqs_res, const SMPIOCapability ioCap_res, const SMPOOBDataFlag oobFlag_res) noexcept
{
    // BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.1 Security Properties

    if( !use_sc ) {
        // BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.1 Selecting key generation method Table 2.6 (STK, le_sc_all_supported==false)
        // LE Secure Connections is _NOT_ supported by both devices.

        // Authenticated via OOB, if both support OOB
        if( SMPOOBDataFlag::OOB_AUTH_DATA_REMOTE_PRESENT == oobFlag_ini &&
            SMPOOBDataFlag::OOB_AUTH_DATA_REMOTE_PRESENT == oobFlag_res )
        {
            return PairingMode::OUT_OF_BAND;
        }

        // Authenticated via IOCapabilities, if any of them has requested MITM
        if( isSMPAuthReqBitSet( authReqs_ini, SMPAuthReqs::MITM ) ||
            isSMPAuthReqBitSet( authReqs_res, SMPAuthReqs::MITM ) )
        {
            return getPairingMode(use_sc, ioCap_ini, ioCap_res);
        }

        // Unauthenticated pairing
        return PairingMode::JUST_WORKS;

    } else {
        // BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.1 Selecting key generation method Table 2.7 (LTK, le_sc_all_supported==true)
        // LE Secure Connections is supported by both devices.

        // Authenticated via OOB, if any of them supports OOB
        if( SMPOOBDataFlag::OOB_AUTH_DATA_REMOTE_PRESENT == oobFlag_ini ||
            SMPOOBDataFlag::OOB_AUTH_DATA_REMOTE_PRESENT == oobFlag_res )
        {
            return PairingMode::OUT_OF_BAND;
        }

        // Authenticated via IOCapabilities, if any of them has requested MITM
        if( isSMPAuthReqBitSet( authReqs_ini, SMPAuthReqs::MITM ) ||
            isSMPAuthReqBitSet( authReqs_res, SMPAuthReqs::MITM ) )
        {
            return getPairingMode(use_sc, ioCap_ini, ioCap_res);
        }

        // Unauthenticated pairing
        return PairingMode::JUST_WORKS;
    }
}

/**
 * Mapping SMPIOCapability from initiator and responder to PairingMode.
 *
 * Notable, the following is deduced from
 * BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.1 Selecting key generation method Table 2.8
 * and differs a little from BlueZ smp.c implementation.
 *
 * Index values, using SMPIOCapabilities as follows:
        DISPLAY_ONLY                = 0x00,
        DISPLAY_YES_NO              = 0x01,
        KEYBOARD_ONLY               = 0x02,
        NO_INPUT_NO_OUTPUT          = 0x03,
        KEYBOARD_DISPLAY            = 0x04
 *
 */
#define PM_JUST__WORKS PairingMode::JUST_WORKS
#define PM_PASSKEY_INI PairingMode::PASSKEY_ENTRY_ini   // Passkey Entry input by initiator. Responder produces and display artifact.
#define PM_PASSKEY_RES PairingMode::PASSKEY_ENTRY_res   // Passkey Entry input by responder. Initiator produces and display artifact.
#define PM_PASSKEY_ALL PairingMode::PASSKEY_ENTRY_ini   // Passkey Entry input by initiator and responder. Using input from initiator!
#define PM_NUMCOMP_INI PairingMode::NUMERIC_COMPARE_ini // Comparison of PIN input by initiator. Responder produces and display artifact.
#define PM_NUMCOMP_RES PairingMode::NUMERIC_COMPARE_res // Comparison of PIN input by responder. Initiator produces and display artifact.
#define PM_NUMCOMP_ANY PairingMode::NUMERIC_COMPARE_ini // Comparison of PIN input by any device. Using input from initiator!

static const PairingMode legacy_pairing[5 /* ioCap_res */][5 /* ioCap_ini */] = {
 /* Responder / Initiator     DISPLAY_ONLY    DISPLAY_YES_NO  KEYBOARD_ONLY   NO_INPUT_NO_OUT KEYBOARD_DISPLAY */
 /* Res: DISPLAY_ONLY */    { PM_JUST__WORKS, PM_JUST__WORKS, PM_PASSKEY_INI, PM_JUST__WORKS, PM_PASSKEY_INI },
 /* Res: DISPLAY_YES_NO */  { PM_JUST__WORKS, PM_JUST__WORKS, PM_PASSKEY_INI, PM_JUST__WORKS, PM_PASSKEY_INI },
 /* Res: KEYBOARD_ONLY */   { PM_PASSKEY_RES, PM_PASSKEY_RES, PM_PASSKEY_ALL, PM_JUST__WORKS, PM_PASSKEY_RES },
 /* Res: NO_INPUT_NO_OUTP */{ PM_JUST__WORKS, PM_JUST__WORKS, PM_JUST__WORKS, PM_JUST__WORKS, PM_JUST__WORKS },
 /* Res: KEYBOARD_DISPLAY */{ PM_PASSKEY_RES, PM_PASSKEY_RES, PM_PASSKEY_INI, PM_JUST__WORKS, PM_PASSKEY_RES },
};
static const PairingMode seccon_pairing[5 /* ioCap_res */][5 /* ioCap_ini */] = {
 /* Responder / Initiator     DISPLAY_ONLY    DISPLAY_YES_NO  KEYBOARD_ONLY   NO_INPUT_NO_OUT KEYBOARD_DISPLAY */
 /* Res: DISPLAY_ONLY */    { PM_JUST__WORKS, PM_JUST__WORKS, PM_PASSKEY_INI, PM_JUST__WORKS, PM_PASSKEY_INI },
 /* Res: DISPLAY_YES_NO */  { PM_JUST__WORKS, PM_NUMCOMP_ANY, PM_PASSKEY_INI, PM_JUST__WORKS, PM_NUMCOMP_ANY },
 /* Res: KEYBOARD_ONLY */   { PM_PASSKEY_RES, PM_PASSKEY_RES, PM_PASSKEY_ALL, PM_JUST__WORKS, PM_PASSKEY_RES },
 /* Res: NO_INPUT_NO_OUTP */{ PM_JUST__WORKS, PM_JUST__WORKS, PM_JUST__WORKS, PM_JUST__WORKS, PM_JUST__WORKS },
 /* Res: KEYBOARD_DISPLAY */{ PM_PASSKEY_RES, PM_NUMCOMP_ANY, PM_PASSKEY_INI, PM_JUST__WORKS, PM_NUMCOMP_ANY },
};

PairingMode direct_bt::getPairingMode(const bool use_sc,
                                      const SMPIOCapability ioCap_ini, const SMPIOCapability ioCap_res) noexcept
{
    // BT Core Spec v5.2: Vol 3, Part H (SM): 2.3.5.1 Selecting key generation method Table 2.8
    const uint8_t ioCap_ini_int = number(ioCap_ini);
    const uint8_t ioCap_res_int = number(ioCap_res);
    if( ioCap_ini_int > 4) {
        ABORT("Invalid ioCap_init %s, %d", getSMPIOCapabilityString(ioCap_ini).c_str(), ioCap_ini_int);
    }
    if( ioCap_res_int > 4) {
        ABORT("Invalid ioCap_resp %s, %d", getSMPIOCapabilityString(ioCap_res).c_str(), ioCap_res_int);
    }
    if( use_sc ) {
        return seccon_pairing[ioCap_res_int][ioCap_ini_int];
    } else {
        return legacy_pairing[ioCap_res_int][ioCap_ini_int];
    }
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

#define CASE_TO_STRING_OPCODE(V) case Opcode::V: return #V;

std::string SMPPDUMsg::getOpcodeString(const Opcode opc) noexcept {
    switch(opc) {
        OPCODE_ENUM(CASE_TO_STRING_OPCODE)
        default: ; // fall through intended
    }
    return "Unknown SMP Opcode";
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

#define CASE_TO_STRING_KEYDISTFMT(V) case KeyDistFormat::V: return #V;

std::string SMPPairingMsg::getKeyDistFormatBitString(const KeyDistFormat bit) noexcept {
    switch(bit) {
        KEYDISTFMT_ENUM(CASE_TO_STRING_KEYDISTFMT)
        default: ; // fall through intended
    }
    return "Unknown SMP KeyDistributionFormat bit";
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

#define CASE_TO_STRING_TYPECODE(V) case TypeCode::V: return #V;

std::string SMPPasskeyNotification::getTypeCodeString(const TypeCode tc) noexcept {
    switch(tc) {
        TYPECODE_ENUM(CASE_TO_STRING_TYPECODE)
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
        case Opcode::PAIRING_CONFIRM:               res = new SMPPairConfirmMsg(buffer, buffer_size); break;
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
