/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2021 Gothel Software e.K.
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
#include <sstream>

#include  <algorithm>

#include <jau/debug.hpp>
#include <jau/darray.hpp>

#include "BTTypes0.hpp"

using namespace direct_bt;

template<typename T>
static void append_bitstr(std::string& out, T mask, T bit, const std::string& bitstr, bool& comma) {
    if( bit == ( mask & bit ) ) {
        if( comma ) { out.append(", "); }
        out.append(bitstr); comma = true;
    }
}
#define APPEND_BITSTR(U,V,M) append_bitstr(out, M, U::V, #V, comma);

#define CASE_TO_STRING(V) case V: return #V;
#define CASE2_TO_STRING(U,V) case U::V: return #V;

BDAddressType direct_bt::to_BDAddressType(const HCILEPeerAddressType hciPeerAddrType) noexcept {
    switch(hciPeerAddrType) {
        case HCILEPeerAddressType::PUBLIC:
            return BDAddressType::BDADDR_LE_PUBLIC;
        case HCILEPeerAddressType::RANDOM:
            [[fallthrough]];
        case HCILEPeerAddressType::PUBLIC_IDENTITY:
            [[fallthrough]];
        case HCILEPeerAddressType::RANDOM_STATIC_IDENTITY:
            return BDAddressType::BDADDR_LE_RANDOM;
        default:
            return BDAddressType::BDADDR_UNDEFINED;
    }
}

#define CHAR_DECL_HCILEPeerAddressType_ENUM(X) \
        X(HCILEPeerAddressType,PUBLIC) \
        X(HCILEPeerAddressType,RANDOM) \
        X(HCILEPeerAddressType,PUBLIC_IDENTITY) \
        X(HCILEPeerAddressType,RANDOM_STATIC_IDENTITY) \
        X(HCILEPeerAddressType,UNDEFINED)

std::string direct_bt::to_string(const HCILEPeerAddressType type) noexcept {
    switch(type) {
        CHAR_DECL_HCILEPeerAddressType_ENUM(CASE2_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown HCILEPeerAddressType "+jau::to_hexstring(number(type));
}

BDAddressType direct_bt::to_BDAddressType(const HCILEOwnAddressType hciOwnAddrType) noexcept {
    switch(hciOwnAddrType) {
        case HCILEOwnAddressType::PUBLIC:
            return BDAddressType::BDADDR_LE_PUBLIC;
        case HCILEOwnAddressType::RANDOM:
            [[fallthrough]];
        case HCILEOwnAddressType::RESOLVABLE_OR_PUBLIC:
            [[fallthrough]];
        case HCILEOwnAddressType::RESOLVABLE_OR_RANDOM:
            return BDAddressType::BDADDR_LE_RANDOM;
        default:
            return BDAddressType::BDADDR_UNDEFINED;
    }
}

HCILEOwnAddressType direct_bt::to_HCILEOwnAddressType(const BDAddressType addrType, bool resolvable) noexcept {
    switch(addrType) {
        case BDAddressType::BDADDR_LE_PUBLIC:
            return HCILEOwnAddressType::PUBLIC;

        case BDAddressType::BDADDR_LE_RANDOM:
            /** FIXME: Sufficient mapping for adapter put in random address mode? */
            return resolvable ? HCILEOwnAddressType::RESOLVABLE_OR_RANDOM : HCILEOwnAddressType::RANDOM;

        case BDAddressType::BDADDR_BREDR:
            [[fallthrough]];
        case BDAddressType::BDADDR_UNDEFINED:
            [[fallthrough]];
        default:
            return HCILEOwnAddressType::UNDEFINED;
    }
}

#define CHAR_DECL_HCILEOwnAddressType_ENUM(X) \
        X(HCILEOwnAddressType,PUBLIC) \
        X(HCILEOwnAddressType,RANDOM) \
        X(HCILEOwnAddressType,RESOLVABLE_OR_PUBLIC) \
        X(HCILEOwnAddressType,RESOLVABLE_OR_RANDOM) \
        X(HCILEOwnAddressType,UNDEFINED)

std::string direct_bt::to_string(const HCILEOwnAddressType type) noexcept {
    switch(type) {
        CHAR_DECL_HCILEOwnAddressType_ENUM(CASE2_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown HCILEOwnAddressType "+jau::to_hexstring(number(type));
}


#define CHAR_DECL_BDADDRESSTYPE_ENUM(X) \
        X(BDAddressType,BDADDR_BREDR) \
        X(BDAddressType,BDADDR_LE_PUBLIC) \
        X(BDAddressType,BDADDR_LE_RANDOM) \
        X(BDAddressType,BDADDR_UNDEFINED)

std::string direct_bt::to_string(const BDAddressType type) noexcept {
    switch(type) {
        CHAR_DECL_BDADDRESSTYPE_ENUM(CASE2_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown BDAddressType "+jau::to_hexstring(number(type));
}

#define CHAR_DECL_LERANDOMADDRESSTYPE_ENUM(X) \
        X(BLERandomAddressType,UNRESOLVABLE_PRIVAT) \
        X(BLERandomAddressType,RESOLVABLE_PRIVAT) \
        X(BLERandomAddressType,RESERVED) \
        X(BLERandomAddressType,STATIC_PUBLIC) \
        X(BLERandomAddressType,UNDEFINED)

std::string direct_bt::to_string(const BLERandomAddressType type) noexcept {
    switch(type) {
        CHAR_DECL_LERANDOMADDRESSTYPE_ENUM(CASE2_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown BLERandomAddressType "+jau::to_hexstring(number(type));
}

BLERandomAddressType BDAddressAndType::getBLERandomAddressType(const jau::EUI48& address, const BDAddressType addressType) noexcept {
    if( BDAddressType::BDADDR_LE_RANDOM != addressType ) {
        return BLERandomAddressType::UNDEFINED;
    }
    const uint8_t high2 = ( address.b[5] >> 6 ) & 0x03;
    switch( high2 ) {
        case 0x00: return BLERandomAddressType::UNRESOLVABLE_PRIVAT;
        case 0x01: return BLERandomAddressType::RESOLVABLE_PRIVAT;
        case 0x02: return BLERandomAddressType::RESERVED;
        case 0x03: return BLERandomAddressType::STATIC_PUBLIC;
        default: return BLERandomAddressType::UNDEFINED;
    }
}

std::string BDAddressAndType::getBLERandomAddressTypeString(const jau::EUI48& address, const BDAddressType addressType, const std::string& prefix) noexcept {
    if( BDAddressType::BDADDR_LE_RANDOM != addressType ) {
        return std::string();
    }
    return prefix+to_string( BDAddressAndType::getBLERandomAddressType(address, addressType) );
}

const BDAddressAndType direct_bt::BDAddressAndType::ANY_BREDR_DEVICE(jau::EUI48::ANY_DEVICE, BDAddressType::BDADDR_BREDR);
const BDAddressAndType direct_bt::BDAddressAndType::ANY_DEVICE(jau::EUI48::ANY_DEVICE, BDAddressType::BDADDR_UNDEFINED);

std::string BDAddressAndType::toString() const noexcept {
    return "["+address.toString()+", "+to_string(type)+getBLERandomAddressTypeString(address, type, ", ")+"]";
}

// *************************************************
// *************************************************
// *************************************************

static inline const int8_t * const_uint8_to_const_int8_ptr(const uint8_t* p) noexcept {
    return static_cast<const int8_t *>( static_cast<void *>( const_cast<uint8_t*>( p ) ) );
}

std::string direct_bt::to_string(const BTRole v) noexcept {
    switch(v) {
        case BTRole::None: return "None";
        case BTRole::Master: return "Master";
        case BTRole::Slave: return "Slave";
    }
    return "Unknown BTRole "+jau::to_hexstring(number(v));
}

std::string direct_bt::to_string(const GATTRole v) noexcept {
    switch(v) {
        case GATTRole::None: return "None";
        case GATTRole::Client: return "Client";
        case GATTRole::Server: return "Server";
    }
    return "Unknown GATTRole "+jau::to_hexstring(number(v));
}

std::string direct_bt::to_string(const BTMode v) noexcept {
    switch(v) {
        case BTMode::NONE: return "NONE";
        case BTMode::DUAL: return "DUAL";
        case BTMode::BREDR: return "BREDR";
        case BTMode::LE: return "LE";
    }
    return "Unknown BTMode "+jau::to_hexstring(number(v));
}

BTMode direct_bt::to_BTMode(const std::string & value) noexcept {
    if( "DUAL" == value ) {
        return BTMode::DUAL;
    }
    if( "BREDR" == value ) {
        return BTMode::BREDR;
    }
    if( "LE" == value ) {
        return BTMode::LE;
    }
    return BTMode::NONE;
}

// *************************************************
// *************************************************
// *************************************************

#define LEFEATURES_ENUM(X,M) \
    X(LE_Features,LE_Encryption,M) \
    X(LE_Features,Conn_Param_Req_Proc,M) \
    X(LE_Features,Ext_Rej_Ind,M) \
    X(LE_Features,SlaveInit_Feat_Exchg,M) \
    X(LE_Features,LE_Ping,M) \
    X(LE_Features,LE_Data_Pkt_Len_Ext,M) \
    X(LE_Features,LL_Privacy,M) \
    X(LE_Features,Ext_Scan_Filter_Pol,M) \
    X(LE_Features,LE_2M_PHY,M) \
    X(LE_Features,Stable_Mod_Idx_Tx,M) \
    X(LE_Features,Stable_Mod_Idx_Rx,M) \
    X(LE_Features,LE_Coded_PHY,M) \
    X(LE_Features,LE_Ext_Adv,M) \
    X(LE_Features,LE_Per_Adv,M) \
    X(LE_Features,Chan_Sel_Algo_2,M) \
    X(LE_Features,LE_Pwr_Cls_1,M) \
    X(LE_Features,Min_Num_Used_Chan_Proc,M) \
    X(LE_Features,Conn_CTE_Req,M) \
    X(LE_Features,Conn_CTE_Res,M) \
    X(LE_Features,ConnLess_CTE_Tx,M) \
    X(LE_Features,ConnLess_CTE_Rx,M) \
    X(LE_Features,AoD,M) \
    X(LE_Features,AoA,M) \
    X(LE_Features,Rx_Const_Tone_Ext,M) \
    X(LE_Features,Per_Adv_Sync_Tx_Sender,M) \
    X(LE_Features,Per_Adv_Sync_Tx_Rec,M) \
    X(LE_Features,Zzz_Clk_Acc_Upd,M) \
    X(LE_Features,Rem_Pub_Key_Val,M) \
    X(LE_Features,Conn_Iso_Stream_Master,M) \
    X(LE_Features,Conn_Iso_Stream_Slave,M) \
    X(LE_Features,Iso_Brdcst,M) \
    X(LE_Features,Sync_Rx,M) \
    X(LE_Features,Iso_Chan,M) \
    X(LE_Features,LE_Pwr_Ctrl_Req,M) \
    X(LE_Features,LE_Pwr_Chg_Ind,M) \
    X(LE_Features,LE_Path_Loss_Mon,M)

std::string direct_bt::to_string(const LE_Features mask) noexcept {
    std::string out("[");
    bool comma = false;
    LEFEATURES_ENUM(APPEND_BITSTR,mask)
    out.append("]");
    return out;
}

// *************************************************
// *************************************************
// *************************************************

#define LE_PHYs_ENUM(X,M) \
    X(LE_PHYs,LE_1M,M) \
    X(LE_PHYs,LE_2M,M) \
    X(LE_PHYs,LE_CODED,M)

std::string direct_bt::to_string(const LE_PHYs mask) noexcept {
    std::string out("[");
    bool comma = false;
    LE_PHYs_ENUM(APPEND_BITSTR,mask)
    out.append("]");
    return out;
}

// *************************************************
// *************************************************
// *************************************************

std::string direct_bt::to_string(const BTSecurityLevel v) noexcept {
    switch(v) {
        case BTSecurityLevel::UNSET:         return "UNSET";
        case BTSecurityLevel::NONE:          return "NONE";
        case BTSecurityLevel::ENC_ONLY:      return "ENC_ONLY";
        case BTSecurityLevel::ENC_AUTH:      return "ENC_AUTH";
        case BTSecurityLevel::ENC_AUTH_FIPS: return "ENC_AUTH_FIPS";
    }
    return "Unknown BTSecurityLevel "+jau::to_hexstring(number(v));
}

std::string direct_bt::to_string(const PairingMode v) noexcept {
    switch(v) {
        case PairingMode::NONE:                return "NONE";
        case PairingMode::NEGOTIATING:         return "NEGOTIATING";
        case PairingMode::JUST_WORKS:          return "JUST_WORKS";
        case PairingMode::PASSKEY_ENTRY_ini:   return "PASSKEY_ini";
        case PairingMode::PASSKEY_ENTRY_res:   return "PASSKEY_res";
        case PairingMode::NUMERIC_COMPARE_ini: return "NUMCOMP_ini";
        case PairingMode::NUMERIC_COMPARE_res: return "NUMCOMP_res";
        case PairingMode::OUT_OF_BAND:         return "OUT_OF_BAND";
        case PairingMode::PRE_PAIRED:          return "PRE_PAIRED";
    }
    return "Unknown PairingMode "+jau::to_hexstring(number(v));
}

ScanType direct_bt::to_ScanType(BTMode btMode) {
    switch ( btMode ) {
        case BTMode::DUAL:
            return ScanType::DUAL;
        case BTMode::BREDR:
            return ScanType::BREDR;
        case BTMode::LE:
            return ScanType::LE;
        default:
            throw jau::IllegalArgumentException("Unsupported BTMode "+to_string(btMode), E_FILE_LINE);
    }
}

#define SCANTYPE_ENUM(X) \
        X(NONE) \
        X(BREDR) \
        X(LE) \
        X(DUAL)

#define SCANTYPE_CASE_TO_STRING(V) case ScanType::V: return #V;

std::string direct_bt::to_string(const ScanType v) noexcept {
    switch(v) {
        SCANTYPE_ENUM(SCANTYPE_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown ScanType "+jau::to_hexstring(number(v));
}

#define AD_PDU_Type_ENUM(X) \
    X(ADV_IND) \
    X(ADV_DIRECT_IND) \
    X(ADV_SCAN_IND) \
    X(ADV_NONCONN_IND) \
    X(SCAN_RSP) \
    X(ADV_IND2) \
    X(DIRECT_IND2) \
    X(SCAN_IND2) \
    X(NONCONN_IND2) \
    X(SCAN_RSP_to_ADV_IND) \
    X(SCAN_RSP_to_ADV_SCAN_IND) \
    X(UNDEFINED)

#define AD_PDU_Type_CASE_TO_STRING(V) case AD_PDU_Type::V: return #V;

std::string direct_bt::to_string(const AD_PDU_Type v) noexcept {
    switch(v) {
        AD_PDU_Type_ENUM(AD_PDU_Type_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown AD_PDU_Type "+jau::to_hexstring(number(v));
}

// *************************************************
// *************************************************
// *************************************************

#define EAD_Event_Type_ENUM(X,M) \
    X(EAD_Event_Type,CONN_ADV,M) \
    X(EAD_Event_Type,SCAN_ADV,M) \
    X(EAD_Event_Type,DIR_ADV,M) \
    X(EAD_Event_Type,SCAN_RSP,M) \
    X(EAD_Event_Type,LEGACY_PDU,M) \
    X(EAD_Event_Type,DATA_B0,M) \
    X(EAD_Event_Type,DATA_B1,M)

std::string direct_bt::to_string(const EAD_Event_Type mask) noexcept {
    std::string out("[");
    bool comma = false;
    EAD_Event_Type_ENUM(APPEND_BITSTR,mask)
    out.append("]");
    return out;
}

// *************************************************
// *************************************************
// *************************************************

#define L2CAP_CID_ENUM(X) \
    X(UNDEFINED) \
    X(SIGNALING) \
    X(CONN_LESS) \
    X(A2MP) \
    X(ATT) \
    X(LE_SIGNALING) \
    X(SMP) \
    X(SMP_BREDR) \
    X(DYN_START) \
    X(DYN_END) \
    X(LE_DYN_END)

#define L2CAP_CID_CASE_TO_STRING(V) case L2CAP_CID::V: return #V;

std::string direct_bt::to_string(const L2CAP_CID v) noexcept {
    switch(v) {
        L2CAP_CID_ENUM(L2CAP_CID_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown L2CAP_CID "+jau::to_hexstring(number(v));
}

#define L2CAP_PSM_ENUM(X) \
    X(UNDEFINED) \
    X(SDP) \
    X(RFCOMM) \
    X(TCSBIN) \
    X(TCSBIN_CORDLESS) \
    X(BNEP) \
    X(HID_CONTROL) \
    X(HID_INTERRUPT) \
    X(UPNP) \
    X(AVCTP) \
    X(AVDTP) \
    X(AVCTP_BROWSING) \
    X(UDI_C_PLANE) \
    X(ATT) \
    X(LE_DYN_START) \
    X(LE_DYN_END) \
    X(DYN_START) \
    X(DYN_END) \
    X(AUTO_END)

#define L2CAP_PSM_CASE_TO_STRING(V) case L2CAP_PSM::V: return #V;

std::string direct_bt::to_string(const L2CAP_PSM v) noexcept {
    switch(v) {
        L2CAP_PSM_ENUM(L2CAP_PSM_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown L2CAP_PSM "+jau::to_hexstring(number(v));
}

#define APPEARANCECAT_ENUM(X) \
    X(UNKNOWN) \
    X(GENERIC_PHONE) \
    X(GENERIC_COMPUTER) \
    X(GENERIC_WATCH) \
    X(SPORTS_WATCH) \
    X(GENERIC_CLOCK) \
    X(GENERIC_DISPLAY) \
    X(GENERIC_REMOTE_CLOCK) \
    X(GENERIC_EYE_GLASSES) \
    X(GENERIC_TAG) \
    X(GENERIC_KEYRING) \
    X(GENERIC_MEDIA_PLAYER) \
    X(GENERIC_BARCODE_SCANNER) \
    X(GENERIC_THERMOMETER) \
    X(GENERIC_THERMOMETER_EAR) \
    X(GENERIC_HEART_RATE_SENSOR) \
    X(HEART_RATE_SENSOR_BELT) \
    X(GENERIC_BLOD_PRESSURE) \
    X(BLOD_PRESSURE_ARM) \
    X(BLOD_PRESSURE_WRIST) \
    X(HID) \
    X(HID_KEYBOARD) \
    X(HID_MOUSE) \
    X(HID_JOYSTICK) \
    X(HID_GAMEPAD) \
    X(HID_DIGITIZER_TABLET) \
    X(HID_CARD_READER) \
    X(HID_DIGITAL_PEN) \
    X(HID_BARCODE_SCANNER) \
    X(GENERIC_GLUCOSE_METER) \
    X(GENERIC_RUNNING_WALKING_SENSOR) \
    X(RUNNING_WALKING_SENSOR_IN_SHOE) \
    X(RUNNING_WALKING_SENSOR_ON_SHOE) \
    X(RUNNING_WALKING_SENSOR_HIP) \
    X(GENERIC_CYCLING) \
    X(CYCLING_COMPUTER) \
    X(CYCLING_SPEED_SENSOR) \
    X(CYCLING_CADENCE_SENSOR) \
    X(CYCLING_POWER_SENSOR) \
    X(CYCLING_SPEED_AND_CADENCE_SENSOR) \
    X(GENERIC_PULSE_OXIMETER) \
    X(PULSE_OXIMETER_FINGERTIP) \
    X(PULSE_OXIMETER_WRIST) \
    X(GENERIC_WEIGHT_SCALE) \
    X(GENERIC_PERSONAL_MOBILITY_DEVICE) \
    X(PERSONAL_MOBILITY_DEVICE_WHEELCHAIR) \
    X(PERSONAL_MOBILITY_DEVICE_SCOOTER) \
    X(GENERIC_CONTINUOUS_GLUCOSE_MONITOR) \
    X(GENERIC_INSULIN_PUMP) \
    X(INSULIN_PUMP_DURABLE) \
    X(INSULIN_PUMP_PATCH) \
    X(INSULIN_PUMP_PEN) \
    X(GENERIC_MEDICATION_DELIVERY) \
    X(GENERIC_OUTDOOR_SPORTS_ACTIVITY) \
    X(OUTDOOR_SPORTS_ACTIVITY_LOCATION_DISPLAY_DEVICE) \
    X(OUTDOOR_SPORTS_ACTIVITY_LOCATION_AND_NAVIGATION_DISPLAY_DEVICE) \
    X(OUTDOOR_SPORTS_ACTIVITY_LOCATION_POD) \
    X(OUTDOOR_SPORTS_ACTIVITY_LOCATION_AND_NAVIGATION_POD) \

#define APPEARANCE_CASE_TO_STRING(V) case AppearanceCat::V: return #V;

std::string direct_bt::to_string(const AppearanceCat v) noexcept {
    switch(v) {
        APPEARANCECAT_ENUM(APPEARANCE_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown AppearanceCat "+jau::to_hexstring(number(v));
}

// *************************************************
// *************************************************
// *************************************************

static std::string bt_compidtostr(const uint16_t companyid) noexcept {
    return std::to_string(companyid);
}

ManufactureSpecificData::ManufactureSpecificData(uint16_t const company_)
: company(company_), companyName(std::string(bt_compidtostr(company_))),
  data(jau::lb_endian::little /* intentional zero sized */)
{ }

ManufactureSpecificData::ManufactureSpecificData(uint16_t const company_, uint8_t const * const data_, jau::nsize_t const data_len)
: company(company_), companyName(std::string(bt_compidtostr(company_))),
  data(data_, data_len, jau::lb_endian::little)
{ }

std::string ManufactureSpecificData::toString() const noexcept {
  std::string out("MSD[company[");
  out.append(std::to_string(company)+" "+companyName);
  out.append("], data["+data.toString()+"]]");
  return out;
}

// *************************************************
// *************************************************
// *************************************************

#define GAPFLAGS_ENUM(X,M) \
    X(GAPFlags,LE_Ltd_Disc,M) \
    X(GAPFlags,LE_Gen_Disc,M) \
    X(GAPFlags,BREDR_UNSUP,M) \
    X(GAPFlags,Dual_SameCtrl,M) \
    X(GAPFlags,Dual_SameHost,M) \
    X(GAPFlags,RESERVED1,M) \
    X(GAPFlags,RESERVED2,M) \
    X(GAPFlags,RESERVED3,M)

std::string direct_bt::to_string(const GAPFlags v) noexcept {
    std::string out("[");
    bool comma = false;
    GAPFLAGS_ENUM(APPEND_BITSTR,v)
    out.append("]");
    return out;
}

// *************************************************
// *************************************************
// *************************************************

#define EIRDATATYPE_ENUM(X,M) \
    X(EIRDataType,EVT_TYPE,M) \
    X(EIRDataType,EXT_EVT_TYPE,M) \
    X(EIRDataType,BDADDR_TYPE,M) \
    X(EIRDataType,BDADDR,M) \
    X(EIRDataType,FLAGS,M) \
    X(EIRDataType,NAME,M) \
    X(EIRDataType,NAME_SHORT,M) \
    X(EIRDataType,RSSI,M) \
    X(EIRDataType,TX_POWER,M) \
    X(EIRDataType,MANUF_DATA,M) \
    X(EIRDataType,DEVICE_CLASS,M) \
    X(EIRDataType,APPEARANCE,M) \
    X(EIRDataType,HASH,M) \
    X(EIRDataType,RANDOMIZER,M) \
    X(EIRDataType,DEVICE_ID,M) \
    X(EIRDataType,CONN_IVAL,M) \
    X(EIRDataType,SERVICE_UUID,M) \
    X(EIRDataType,ALL,M)

std::string direct_bt::to_string(const EIRDataType mask) noexcept {
    std::string out("[");
    bool comma = false;
    EIRDATATYPE_ENUM(APPEND_BITSTR,mask)
    out.append("]");
    return out;
}

// *************************************************
// *************************************************
// *************************************************

void EInfoReport::clear() noexcept {
    EInfoReport eir_clean;
    *this = eir_clean;
}

EIRDataType EInfoReport::set(const EInfoReport& eir) noexcept {
    EIRDataType res = EIRDataType::NONE;

    if( eir.isSet( EIRDataType::EVT_TYPE ) ) {
        if( !isSet( EIRDataType::EVT_TYPE ) || getEvtType() != eir.getEvtType() ) {
            setEvtType(eir.getEvtType());
            direct_bt::set(res, EIRDataType::EVT_TYPE);
        }
    }
    if( eir.isSet( EIRDataType::EXT_EVT_TYPE ) ) {
        if( !isSet( EIRDataType::EXT_EVT_TYPE ) || getExtEvtType() != eir.getExtEvtType() ) {
            setExtEvtType(eir.getExtEvtType());
            direct_bt::set(res, EIRDataType::EXT_EVT_TYPE);
        }
    }
    if( eir.isSet( EIRDataType::BDADDR_TYPE ) ) {
        if( !isSet( EIRDataType::BDADDR_TYPE ) || getAddressType() != eir.getAddressType() ) {
            setAddressType(eir.getAddressType());
            direct_bt::set(res, EIRDataType::BDADDR_TYPE);
        }
    }
    if( eir.isSet( EIRDataType::BDADDR ) ) {
        if( !isSet( EIRDataType::BDADDR ) || getAddress() != eir.getAddress() ) {
            setAddress(eir.getAddress());
            direct_bt::set(res, EIRDataType::BDADDR);
        }
    }
    if( eir.isSet( EIRDataType::RSSI ) ) {
        if( !isSet( EIRDataType::RSSI ) || getRSSI() != eir.getRSSI() ) {
            setRSSI(eir.getRSSI());
            direct_bt::set(res, EIRDataType::RSSI);
        }
    }
    if( eir.isSet( EIRDataType::TX_POWER ) ) {
        if( !isSet( EIRDataType::TX_POWER ) || getTxPower() != eir.getTxPower() ) {
            setTxPower(eir.getTxPower());
            direct_bt::set(res, EIRDataType::TX_POWER);
        }
    }
    if( eir.isSet( EIRDataType::FLAGS ) ) {
        if( !isSet( EIRDataType::FLAGS ) || getFlags() != eir.getFlags() ) {
            addFlags(eir.getFlags());
            direct_bt::set(res, EIRDataType::FLAGS);
        }
    }
    if( eir.isSet( EIRDataType::NAME) ) {
        if( !isSet( EIRDataType::NAME ) || getName() != eir.getName() ) {
            setName(eir.getName());
            direct_bt::set(res, EIRDataType::NAME);
        }
    }
    if( eir.isSet( EIRDataType::NAME_SHORT) ) {
        if( !isSet( EIRDataType::NAME_SHORT ) || getShortName() != eir.getShortName() ) {
            setShortName(eir.getShortName());
            direct_bt::set(res, EIRDataType::NAME_SHORT);
        }
    }
    if( eir.isSet( EIRDataType::MANUF_DATA) ) {
        std::shared_ptr<ManufactureSpecificData> o_msd = eir.getManufactureSpecificData();
        if( nullptr != o_msd && ( !isSet( EIRDataType::MANUF_DATA ) || nullptr == getManufactureSpecificData() || *getManufactureSpecificData() != *o_msd ) ) {
            setManufactureSpecificData(*o_msd);
            direct_bt::set(res, EIRDataType::MANUF_DATA);
        }
    }
    if( eir.isSet( EIRDataType::SERVICE_UUID) ) {
        const jau::darray<std::shared_ptr<const jau::uuid_t>>& services_ = eir.getServices();
        bool added = false;
        for(const auto& uuid : services_) {
            added = addService(uuid) | added;
        }
        if( added ) {
            setServicesComplete(eir.getServicesComplete());
            direct_bt::set(res, EIRDataType::SERVICE_UUID);
        }
    }
    if( eir.isSet( EIRDataType::DEVICE_CLASS) ) {
        if( !isSet( EIRDataType::DEVICE_CLASS ) || getDeviceClass() != eir.getDeviceClass() ) {
            setDeviceClass(eir.getDeviceClass());
            direct_bt::set(res, EIRDataType::DEVICE_CLASS);
        }
    }
    if( eir.isSet( EIRDataType::APPEARANCE) ) {
        if( !isSet( EIRDataType::APPEARANCE ) || getAppearance() != eir.getAppearance() ) {
            setAppearance(eir.getAppearance());
            direct_bt::set(res, EIRDataType::APPEARANCE);
        }
    }
    if( eir.isSet( EIRDataType::HASH) ) {
        if( !isSet( EIRDataType::HASH ) || getHash() != eir.getHash() ) {
            setHash(eir.getHash().get_ptr());
            direct_bt::set(res, EIRDataType::HASH);
        }
    }
    if( eir.isSet( EIRDataType::RANDOMIZER) ) {
        if( !isSet( EIRDataType::RANDOMIZER ) || getRandomizer() != eir.getRandomizer() ) {
            setRandomizer(eir.getRandomizer().get_ptr());
            direct_bt::set(res, EIRDataType::RANDOMIZER);
        }
    }
    if( eir.isSet( EIRDataType::DEVICE_ID) ) {
        uint16_t source_=0, vendor_=0, product_=0, version_=0;
        eir.getDeviceID(source_, vendor_, product_, version_);
        if( !isSet( EIRDataType::DEVICE_ID ) ||
            did_source != source_ || did_vendor != vendor_ || did_product != product_ || did_version != version_ )
        {
            setDeviceID(source_, vendor_, product_, version_);
            direct_bt::set(res, EIRDataType::DEVICE_ID);
        }
    }
    if( eir.isSet( EIRDataType::CONN_IVAL) ) {
        uint16_t min=0, max=0;
        eir.getConnInterval(min, max);
        if( !isSet( EIRDataType::CONN_IVAL ) || conn_interval_min != min || conn_interval_max != max ) {
            setConnInterval(min, max);
            direct_bt::set(res, EIRDataType::CONN_IVAL);
        }
    }
    if( EIRDataType::NONE != res ) {
        setSource(eir.getSource(), eir.getSourceExt());
        setTimestamp(eir.getTimestamp());
    }
    return res;
}

EInfoReport::ssize_type EInfoReport::findService(const jau::uuid_t& uuid) const noexcept
{
    const size_type size = services.size();
    for (size_type i = 0; i < size; ++i) {
        const std::shared_ptr<const jau::uuid_t> & e = services[i];
        if ( nullptr != e && uuid.equivalent(*e) ) {
            return (ssize_type)i;
        }
    }
    return -1;
}

std::string direct_bt::to_string(EInfoReport::Source source) noexcept {
    switch (source) {
        case EInfoReport::Source::NA: return "N/A";
        case EInfoReport::Source::AD_IND: return "AD_IND";
        case EInfoReport::Source::AD_SCAN_RSP: return "AD_SCAN_RSP";
        case EInfoReport::Source::EIR: return "EIR";
        case EInfoReport::Source::EIR_MGMT: return "EIR_MGMT";
    }
    return "N/A";
}

void EInfoReport::setADAddressType(uint8_t adAddressType) noexcept {
    ad_address_type = adAddressType;
    switch( ad_address_type ) {
        case 0x00: addressType = BDAddressType::BDADDR_LE_PUBLIC; break;
        case 0x01: addressType = BDAddressType::BDADDR_LE_RANDOM; break;
        case 0x02: addressType = BDAddressType::BDADDR_LE_RANDOM; break;
        case 0x03: addressType = BDAddressType::BDADDR_LE_RANDOM; break;
        default: addressType = BDAddressType::BDADDR_UNDEFINED; break;
    }
    set(EIRDataType::BDADDR_TYPE);
}

void EInfoReport::setAddressType(BDAddressType at) noexcept {
    addressType = at;
    switch( addressType ) {
        case BDAddressType::BDADDR_BREDR: ad_address_type = 0; break;
        case BDAddressType::BDADDR_LE_PUBLIC: ad_address_type = 0; break;
        case BDAddressType::BDADDR_LE_RANDOM: ad_address_type = 1; break;
        case BDAddressType::BDADDR_UNDEFINED: ad_address_type = 4; break;
    }
    set(EIRDataType::BDADDR_TYPE);
}

void EInfoReport::setName(const uint8_t *buffer, int buffer_len) noexcept {
    name = jau::get_string(buffer, buffer_len, 30);
    set(EIRDataType::NAME);
}
void EInfoReport::setName(const std::string& name_) noexcept {
    name = name_;
    set(EIRDataType::NAME);
}

void EInfoReport::setShortName(const uint8_t *buffer, int buffer_len) noexcept {
    name_short = jau::get_string(buffer, buffer_len, 30);
    set(EIRDataType::NAME_SHORT);
}
void EInfoReport::setShortName(const std::string& name_short_) noexcept {
    name_short = name_short_;
    set(EIRDataType::NAME_SHORT);
}

void EInfoReport::setManufactureSpecificData(uint16_t const company, uint8_t const * const data, int const data_len) {
    if( nullptr == data || 0 >= data_len ) {
        msd = std::make_shared<ManufactureSpecificData>(company);
    } else {
        msd = std::make_shared<ManufactureSpecificData>(company, data, data_len);
    }
    set(EIRDataType::MANUF_DATA);
}
void EInfoReport::setManufactureSpecificData(const ManufactureSpecificData& msd_) {
    msd = std::make_shared<ManufactureSpecificData>(msd_);
    set(EIRDataType::MANUF_DATA);
}
void EInfoReport::setDeviceID(const uint16_t source_, const uint16_t vendor, const uint16_t product, const uint16_t version) noexcept {
    did_source = source_;
    did_vendor = vendor;
    did_product = product;
    did_version = version;
    set(EIRDataType::DEVICE_ID);
}

bool EInfoReport::addService(const std::shared_ptr<const jau::uuid_t>& uuid) noexcept
{
    auto begin = services.begin();
    auto it = std::find_if(begin, services.end(), [&](std::shared_ptr<const jau::uuid_t> const& p) {
        return nullptr != p && uuid->equivalent(*p);
    });
    if ( it == std::end(services) ) {
        services.push_back(uuid);
        set(EIRDataType::SERVICE_UUID);
        return true;
    }
    return false;
}
bool EInfoReport::addService(const jau::uuid_t& uuid) noexcept {
    return addService( uuid.clone() );
}

std::string EInfoReport::eirDataMaskToString() const noexcept {
    return std::string("Set"+ direct_bt::to_string( EIR_DATA_TYPE_MASK & eir_data_mask ) );
}
std::string EInfoReport::toString(const bool includeServices) const noexcept {
    const std::string source_ext_s = source_ext ? "bt5" : "bt4";
    std::string out(to_string(source)+
                    "["+source_ext_s+", address["+address.toString()+", "+to_string(getAddressType())+"/"+std::to_string(ad_address_type)+
                    "], "+eirDataMaskToString()+", ");
    if( isSet(EIRDataType::NAME) || isSet(EIRDataType::NAME_SHORT) ) {
        out += "name['"+name+"'/'"+name_short+"'], ";
    }

    if( isSet(EIRDataType::EVT_TYPE) || isSet(EIRDataType::EXT_EVT_TYPE) ) {
        out += "type[evt "+to_string(evt_type)+", ead "+to_string(ead_type)+"], ";
    }
    if( isSet(EIRDataType::FLAGS) ) {
        out += "flags"+to_string(flags)+", ";
    }
    if( isSet(EIRDataType::RSSI) ) {
        out += "rssi "+std::to_string(rssi)+", ";
    }
    if( isSet(EIRDataType::TX_POWER) ) {
        out += "tx-power "+std::to_string(tx_power)+", ";
    }
    if( isSet(EIRDataType::CONN_IVAL) ) {
        std::stringstream conn_s;
        conn_s.precision(4+2);
        conn_s << "conn[" << (1.25f * (float)conn_interval_min) << "ms - " << (1.25f * (float)conn_interval_max) << "ms], ";
        out += conn_s.str();
    }
    if( isSet(EIRDataType::DEVICE_CLASS) ) {
        out += "dev-class "+jau::to_hexstring(device_class)+", ";
    }
    if( isSet(EIRDataType::APPEARANCE) ) {
        out += "appearance "+jau::to_hexstring(static_cast<uint16_t>(appearance))+" ("+to_string(appearance)+"), ";
    }
    if( isSet(EIRDataType::HASH) ) {
        out += "hash["+hash.toString()+"], ";
    }
    if( isSet(EIRDataType::RANDOMIZER) ) {
        out += "randomizer["+randomizer.toString()+"], ";
    }
    if( isSet(EIRDataType::DEVICE_ID) ) {
        out += "device-id[source "+jau::to_hexstring(did_source)+
                ", vendor "+jau::to_hexstring(did_vendor)+
                ", product "+jau::to_hexstring(did_product)+
                ", version "+jau::to_hexstring(did_version)+"], ";
    }
    if( isSet(EIRDataType::SERVICE_UUID) ) {
        out += "services[complete "+std::to_string(services_complete)+", count "+std::to_string(services.size())+"], ";
    }
    if( isSet(EIRDataType::MANUF_DATA) ) {
        std::string msdstr = nullptr != msd ? msd->toString() : "MSD[null]";
        out += msdstr+", ";
    }
    out += "]";

    if( includeServices && services.size() > 0 && isSet(EIRDataType::SERVICE_UUID) ) {
        out.append("\n");
        for(const auto& p : services) {
            out.append("  ").append(p->toUUID128String()).append(", ").append(std::to_string(static_cast<int>(p->getTypeSize()))).append(" bytes\n");
        }
    }
    return out;
}

bool EInfoReport::operator==(const EInfoReport& o) const noexcept {
    if( this == &o ) {
        return true;
    }
    return o.eir_data_mask == eir_data_mask &&
           o.evt_type == evt_type &&
           o.ead_type == ead_type &&
           o.flags == flags &&
           o.ad_address_type == ad_address_type &&
           o.address == address &&
           o.name == name &&
           o.name_short == name_short &&
           o.rssi == rssi &&
           o.tx_power == tx_power &&
           ( ( nullptr == o.msd && nullptr == msd ) ||
             ( nullptr != o.msd && nullptr != msd && *o.msd == *msd )
           ) &&
           o.conn_interval_min == conn_interval_min &&
           o.conn_interval_max == conn_interval_max &&
           o.device_class == device_class &&
           o.appearance == appearance &&
           o.hash == hash &&
           o.randomizer == randomizer &&
           o.did_source == did_source &&
           o.did_vendor == did_vendor &&
           o.did_product == did_product &&
           o.did_version == did_version &&
           o.services.size() == services.size() &&
           std::equal( o.services.cbegin(), o.services.cend(), services.cbegin(),
                       [](const std::shared_ptr<const jau::uuid_t>&a, const std::shared_ptr<const jau::uuid_t>&b)
                       -> bool { return *a == *b; } );
}

std::string EInfoReport::getDeviceIDModalias() const noexcept {
    char *cstr = nullptr;
    int length;

    switch (did_source) {
        case 0x0001:
            length = asprintf(&cstr, "bluetooth:v%04Xp%04Xd%04X", did_vendor, did_product, did_version);
            break;
        case 0x0002:
            length = asprintf(&cstr, "usb:v%04Xp%04Xd%04X", did_vendor, did_product, did_version);
            break;
        default:
            length = asprintf(&cstr, "source<0x%X>:v%04Xp%04Xd%04X", did_source, did_vendor, did_product, did_version);
            break;
    }
    if( 0 >= length ) {
        if( nullptr != cstr ) {
            free(cstr);
        }
        return std::string();
    }
    std::string res(cstr);
    free(cstr);
    return res;
}

// *************************************************
// *************************************************
// *************************************************

int EInfoReport::next_data_elem(uint8_t *eir_elem_len, uint8_t *eir_elem_type, uint8_t const **eir_elem_data,
                               uint8_t const * data, int offset, int const size) noexcept
{
    if (offset < size) {
        uint8_t len = data[offset]; // covers: type + data, less len field itself

        if (len == 0) {
            return 0; // end of significant part
        }

        if (len + offset > size) {
            return -ENOENT;
        }

        *eir_elem_type = data[offset + 1];
        *eir_elem_data = data + offset + 2; // net data ptr
        *eir_elem_len = len - 1; // less type -> net data length

        return offset + 1 + len; // next ad_struct offset: + len + type + data
    }
    return -ENOENT;
}

int EInfoReport::read_data(uint8_t const * data, uint8_t const data_length) noexcept {
    int count = 0;
    int offset = 0;
    uint8_t elem_len, elem_type;
    uint8_t const *elem_data;

    while( 0 < ( offset = next_data_elem( &elem_len, &elem_type, &elem_data, data, offset, data_length ) ) )
    {
        count++;

        // Guaranteed: elem_len >= 0!
        switch ( static_cast<GAP_T>(elem_type) ) {
            case GAP_T::FLAGS:
                if( 1 <= elem_len ) {
                    setFlags(static_cast<GAPFlags>(*elem_data));
                }
                break;

            case GAP_T::UUID16_INCOMPLETE:
                [[fallthrough]];
            case GAP_T::UUID16_COMPLETE:
                setServicesComplete( GAP_T::UUID32_COMPLETE == static_cast<GAP_T>(elem_type) );
                for(int j=0; j<elem_len/2; j++) {
                    const std::shared_ptr<const jau::uuid_t> uuid( std::make_shared<const jau::uuid16_t>(elem_data + j*2, jau::lb_endian::little) );
                    addService( uuid );
                }
                break;

            case GAP_T::UUID32_INCOMPLETE:
                [[fallthrough]];
            case GAP_T::UUID32_COMPLETE:
                setServicesComplete( GAP_T::UUID32_COMPLETE == static_cast<GAP_T>(elem_type) );
                for(int j=0; j<elem_len/4; j++) {
                    const std::shared_ptr<const jau::uuid_t> uuid( std::make_shared<const jau::uuid32_t>(elem_data + j*4, jau::lb_endian::little) );
                    addService( uuid );
                }
                break;

            case GAP_T::UUID128_INCOMPLETE:
                [[fallthrough]];
            case GAP_T::UUID128_COMPLETE:
                setServicesComplete( GAP_T::UUID32_COMPLETE == static_cast<GAP_T>(elem_type) );
                for(int j=0; j<elem_len/16; j++) {
                    const std::shared_ptr<const jau::uuid_t> uuid( std::make_shared<const jau::uuid128_t>(elem_data + j*16, jau::lb_endian::little) );
                    addService( uuid );
                }
                break;

            case GAP_T::NAME_LOCAL_SHORT:
                // INFO: Bluetooth Core Specification V5.2 [Vol. 3, Part C, 8, p 1341]
                // INFO: A remote name request is required to obtain the full name, if needed.
                setShortName(elem_data, elem_len);
                break;

            case GAP_T::NAME_LOCAL_COMPLETE:
                setName(elem_data, elem_len);
                break;

            case GAP_T::TX_POWER_LEVEL:
                if( 1 <= elem_len ) {
                    setTxPower(*const_uint8_to_const_int8_ptr(elem_data));
                }
                break;

            case GAP_T::SSP_CLASS_OF_DEVICE:
                if( 3 <= elem_len ) {
                    setDeviceClass(  elem_data[0] |
                                   ( elem_data[1] << 8 ) |
                                   ( elem_data[2] << 16 ) );
                }
                break;

            case GAP_T::DEVICE_ID:
                if( 8 <= elem_len ) {
                    setDeviceID(
                        data[0] | ( data[1] << 8 ), // source
                        data[2] | ( data[3] << 8 ), // vendor
                        data[4] | ( data[5] << 8 ), // product
                        data[6] | ( data[7] << 8 )); // version
                }
                break;

            case GAP_T::SLAVE_CONN_IVAL_RANGE:
                if( 4 <= elem_len ) {
                    const uint16_t min = jau::get_uint16(elem_data + 0, jau::lb_endian::little);
                    const uint16_t max = jau::get_uint16(elem_data + 2, jau::lb_endian::little);
                    setConnInterval(min, max);
                }
                break;

            case GAP_T::SOLICIT_UUID16:
                [[fallthrough]];
            case GAP_T::SOLICIT_UUID128:
                [[fallthrough]];
            case GAP_T::SVC_DATA_UUID16:
                [[fallthrough]];
            case GAP_T::PUB_TRGT_ADDR:
                [[fallthrough]];
            case GAP_T::RND_TRGT_ADDR:
                break;

            case GAP_T::GAP_APPEARANCE:
                if( 2 <= elem_len ) {
                    setAppearance(static_cast<AppearanceCat>( jau::get_uint16(elem_data + 0, jau::lb_endian::little) ));
                }
                break;

            case GAP_T::SSP_HASH_C192:
                if( 16 <= elem_len ) {
                    setHash(elem_data);
                }
                break;

            case GAP_T::SSP_RANDOMIZER_R192:
                if( 16 <= elem_len ) {
                    setRandomizer(elem_data);
                }
                break;

            case GAP_T::SOLICIT_UUID32:
                [[fallthrough]];
            case GAP_T::SVC_DATA_UUID32:
                [[fallthrough]];
            case GAP_T::SVC_DATA_UUID128:
                break;

            case GAP_T::MANUFACTURE_SPECIFIC:
                if( 2 <= elem_len ) {
                    const uint16_t company = jau::get_uint16(elem_data + 0, jau::lb_endian::little);
                    const int data_size = elem_len-2;
                    setManufactureSpecificData(company, data_size > 0 ? elem_data+2 : nullptr, data_size);
                }
                break;

            default:
                // FIXME: Use a data blob!!!!
                DBG_PRINT("%s-Element @ [%d/%d]: Unhandled type 0x%.2X with %d bytes net\n",
                          to_string(source).c_str(), offset, data_length, elem_type, elem_len);
                break;
        }
    }
    return count;
}

#define _WARN_OOB(a) DBG_PRINT("%s: Out of buffer: count %zd + 1 + ad_sz %zd > data_len %zd -> drop %s\n", (a), count, ad_sz, data_length, toString(true).c_str());

jau::nsize_t EInfoReport::write_data(EIRDataType write_mask, uint8_t * data, jau::nsize_t const data_length) const noexcept {
    jau::nsize_t count = 0;
    uint8_t * data_i = data;
    const EIRDataType mask = write_mask & eir_data_mask;

    if( is_set(mask, EIRDataType::FLAGS) ) {
        const jau::nsize_t ad_sz = 2;
        if( ( count + 1 + ad_sz ) > data_length ) {
            _WARN_OOB("FLAGS");
            return count;
        }
        count    += ad_sz + 1;
        *data_i++ = ad_sz;
        *data_i++ = direct_bt::number( GAP_T::FLAGS );
        *data_i++ = direct_bt::number( getFlags() );
    }
    if( is_set(mask, EIRDataType::NAME) ) {
        const jau::nsize_t ad_sz = 1 + name.size();
        if( ( count + 1 + ad_sz ) > data_length ) {
            _WARN_OOB("NAME");
            return count;
        }
        count    += ad_sz + 1;
        *data_i++ = ad_sz;
        *data_i++ = direct_bt::number( GAP_T::NAME_LOCAL_COMPLETE );
        memcpy(data_i, name.c_str(), ad_sz-1);
        data_i   += ad_sz-1;
    } else if( is_set(mask, EIRDataType::NAME_SHORT) ) {
        const jau::nsize_t ad_sz = 1 + name_short.size();
        if( ( count + 1 + ad_sz ) > data_length ) {
            _WARN_OOB("NAME_SHORT");
            return count;
        }
        count    += ad_sz + 1;
        *data_i++ = ad_sz;
        *data_i++ = direct_bt::number( GAP_T::NAME_LOCAL_SHORT );
        memcpy(data_i, name_short.c_str(), ad_sz-1);
        data_i   += ad_sz-1;
    }
    if( is_set(mask, EIRDataType::MANUF_DATA) && nullptr != msd ) {
        const jau::nsize_t msd_data_sz = msd->getData().size();
        const jau::nsize_t ad_sz = 1 + 2 + msd_data_sz;
        if( ( count + 1 + ad_sz ) > data_length ) {
            _WARN_OOB("MANUF_DATA");
            return count;
        }
        count    += ad_sz + 1;
        *data_i++ = ad_sz;
        *data_i++ = direct_bt::number( GAP_T::MANUFACTURE_SPECIFIC );
        jau::put_uint16(data_i + 0, msd->getCompany(), jau::lb_endian::little);
        data_i += 2;
        if( 0 < msd_data_sz ) {
            memcpy(data_i, msd->getData().get_ptr(), msd_data_sz);
            data_i +=msd_data_sz;
        }
    }
    if( is_set(mask, EIRDataType::SERVICE_UUID) ) {
        jau::darray<std::shared_ptr<const jau::uuid_t>> uuid16s, uuid32s, uuid128s;
        for(const auto& p : services) {
            switch( p->getTypeSizeInt() ) {
                case 2:
                    uuid16s.push_back(p);
                    break;
                case 4:
                    uuid32s.push_back(p);
                    break;
                case 16:
                    uuid128s.push_back(p);
                    break;
                default:
                    WARN_PRINT("Undefined UUID of size %zd: %s -> drop\n", p->getTypeSizeInt(), p->toString().c_str());
            }
        }
        if( uuid16s.size() > 0 ) {
            const jau::nsize_t ad_sz = 1 + uuid16s.size() * 2;
            if( ( count + 1 + ad_sz ) > data_length ) {
                _WARN_OOB("UUID16");
                return count;
            }
            count    += ad_sz + 1;
            *data_i++ = ad_sz;
            *data_i++ = direct_bt::number(services_complete ? GAP_T::UUID16_COMPLETE : GAP_T::UUID16_INCOMPLETE);
            for(const auto& p : uuid16s) {
                data_i += p->put(data_i + 0, jau::lb_endian::little);
            }
        }
        if( uuid32s.size() > 0 ) {
            const jau::nsize_t ad_sz = 1 + uuid32s.size() * 4;
            if( ( count + 1 + ad_sz ) > data_length ) {
                _WARN_OOB("UUID32");
                return count;
            }
            count    += ad_sz + 1;
            *data_i++ = ad_sz;
            *data_i++ = direct_bt::number(services_complete ? GAP_T::UUID32_COMPLETE : GAP_T::UUID32_INCOMPLETE);
            for(const auto& p : uuid32s) {
                data_i += p->put(data_i + 0, jau::lb_endian::little);
            }
        }
        if( uuid128s.size() > 0 ) {
            const jau::nsize_t ad_sz = 1 + uuid128s.size() * 16;
            if( ( count + 1 + ad_sz ) > data_length ) {
                _WARN_OOB("UUID128");
                return count;
            }
            count    += ad_sz + 1;
            *data_i++ = ad_sz;
            *data_i++ = direct_bt::number(services_complete ? GAP_T::UUID128_COMPLETE : GAP_T::UUID128_INCOMPLETE);
            for(const auto& p : uuid128s) {
                data_i += p->put(data_i + 0, jau::lb_endian::little);
            }
        }
    }
    if( is_set(mask, EIRDataType::CONN_IVAL) ) {
        const jau::nsize_t ad_sz = 5;
        if( ( count + 1 + ad_sz ) > data_length ) {
            _WARN_OOB("CONN_IVAL");
            return count;
        }
        count    += ad_sz + 1;
        *data_i++ = ad_sz;
        *data_i++ = direct_bt::number( GAP_T::SLAVE_CONN_IVAL_RANGE );
        jau::put_uint16(data_i + 0, conn_interval_min, jau::lb_endian::little);
        jau::put_uint16(data_i + 2, conn_interval_max, jau::lb_endian::little);
        data_i += 4;
    }
    if( is_set(mask, EIRDataType::TX_POWER) ) {
        const jau::nsize_t ad_sz = 2;
        if( ( count + 1 + ad_sz ) > data_length ) {
            _WARN_OOB("TX_POWER");
            return count;
        }
        count    += ad_sz + 1;
        *data_i++ = ad_sz;
        *data_i++ = direct_bt::number( GAP_T::TX_POWER_LEVEL );
        *data_i++ = getTxPower();
    }
    // TODO:
    // if( isEIRDataTypeSet(mask, EIRDataType::DEVICE_CLASS) ) {}
    // if( isEIRDataTypeSet(mask, EIRDataType::APPEARANCE) ) {}
    // if( isEIRDataTypeSet(mask, EIRDataType::HASH) ) {}
    // if( isEIRDataTypeSet(mask, EIRDataType::RANDOMIZER) ) {}
    // if( isEIRDataTypeSet(mask, EIRDataType::DEVICE_ID) ) {}

#if 0
    // Not required: Mark end of significant part
    if( ( count + 1 ) <= data_length ) {
        count    += 1;
        *data_i++ = 0; // zero length
    }
#endif

    return count;
}
// #define AD_DEBUG 1

EInfoReport::Source EInfoReport::toSource(const AD_PDU_Type type) {
    switch( type ) {
        case AD_PDU_Type::ADV_IND:
            [[fallthrough]];
        case AD_PDU_Type::ADV_DIRECT_IND:
            [[fallthrough]];
        case AD_PDU_Type::ADV_SCAN_IND:
            [[fallthrough]];
        case AD_PDU_Type::ADV_NONCONN_IND:
            [[fallthrough]];
        case AD_PDU_Type::ADV_IND2:
            [[fallthrough]];
        case AD_PDU_Type::DIRECT_IND2:
            [[fallthrough]];
        case AD_PDU_Type::SCAN_IND2:
            [[fallthrough]];
        case AD_PDU_Type::NONCONN_IND2:
            return Source::AD_IND;

        case AD_PDU_Type::SCAN_RSP:
            [[fallthrough]];
        case AD_PDU_Type::SCAN_RSP_to_ADV_IND:
            [[fallthrough]];
        case AD_PDU_Type::SCAN_RSP_to_ADV_SCAN_IND:
            return Source::AD_SCAN_RSP;
        default:
            return Source::NA;
    }
}

EInfoReport::Source EInfoReport::toSource(const EAD_Event_Type type) {
    if( is_set(type, EAD_Event_Type::CONN_ADV) ||
        is_set(type, EAD_Event_Type::SCAN_ADV) ||
        is_set(type, EAD_Event_Type::DIR_ADV) ) {
        return Source::AD_IND;
    }
    if( is_set(type, EAD_Event_Type::SCAN_RSP) ) {
        return Source::AD_SCAN_RSP;
    }
    return Source::NA;
}


jau::darray<std::unique_ptr<EInfoReport>> EInfoReport::read_ad_reports(uint8_t const * data, jau::nsize_t const data_length) noexcept {
    jau::nsize_t const num_reports = (jau::nsize_t) data[0];
    jau::darray<std::unique_ptr<EInfoReport>> ad_reports;

    if( 0 == num_reports || num_reports > 0x19 ) {
        DBG_PRINT("AD-Reports: Invalid reports count: %d", num_reports);
        return ad_reports;
    }
    uint8_t const *limes = data + data_length;
    uint8_t const *i_octets = data + 1;
    uint8_t ad_data_len[0x19];
    jau::nsize_t i;
    const uint64_t timestamp = jau::getCurrentMilliseconds();

    const int seg4_size = 1 + 1 + 6 + 1;

    for(i = 0; i < num_reports && i_octets < limes; i++) { // seg 1
        ad_reports.push_back( std::make_unique<EInfoReport>() );
        ad_reports[i]->setSource(Source::AD_IND, false /* ext */); // first guess
        ad_reports[i]->setTimestamp(timestamp);

        if( i_octets + seg4_size > limes ) {
            const jau::snsize_t bytes_left = static_cast<jau::snsize_t>(limes - i_octets);
            WARN_PRINT("AD-Reports: Insufficient data length (1) %zu: report %zu/%zu: min_data_len %zu > bytes-left %zu (Drop)",
                    data_length, i, num_reports, seg4_size, bytes_left);
            ad_reports.pop_back();
            goto errout;
        }

        // seg 1: 1
        {
            const AD_PDU_Type ad_type = static_cast<AD_PDU_Type>(*i_octets++);
            ad_reports[i]->setEvtType( ad_type );
            ad_reports[i]->setSource( toSource( ad_type ), false /* ext */);
        }

        // seg 2: 1
        ad_reports[i]->setADAddressType(*i_octets++);

        // seg 3: 6
        ad_reports[i]->setAddress( jau::le_to_cpu( *((jau::EUI48 const *)i_octets) ) );
        i_octets += 6;

        // seg 4: 1
        ad_data_len[i] = *i_octets++;

        // seg 5: ADV Response Data (EIR)
        if( i_octets + ad_data_len[i] + 1 > limes ) {
            const jau::snsize_t bytes_left = static_cast<jau::snsize_t>(limes - i_octets);
            WARN_PRINT("AD-Reports: Insufficient data length (2) %zu: report %zu/%zu: eir_data_len + rssi %zu > bytes-left %zu (Drop)",
                    data_length, i, num_reports, (ad_data_len[i] + 1), bytes_left);
            ad_reports.pop_back();
            goto errout;
        }
        if( 0 < ad_data_len[i] ) {
            ad_reports[i]->read_data(i_octets, ad_data_len[i]);
            i_octets += ad_data_len[i];
        }

        // seg 6: 1
        ad_reports[i]->setRSSI(*const_uint8_to_const_int8_ptr(i_octets));
        i_octets++;
    }

errout:
    {
        const jau::snsize_t bytes_left = static_cast<jau::snsize_t>(limes - i_octets);
        const jau::snsize_t bytes_took = static_cast<jau::snsize_t>(i_octets - data);
        if( 0 > bytes_left ) {
            ERR_PRINT("AD-Reports: Buffer overflow: %zu reports, bytes[consumed %zu, left %zu, total %zu]",
                    num_reports, bytes_took, bytes_left, data_length);
        }
#ifdef AD_DEBUG
        else {
            DBG_PRINT("AD-Reports: %zu reports, bytes[consumed %zu, left %zu, total %zu]",
                    num_reports, bytes_took, bytes_left, data_length);
        }
        if( jau::environment::get().debug ) {
            for(i=0; i<num_reports; i++) {
                jau::INFO_PRINT("AD[%d]: ad_data_length %d, %s\n", (int)i, (int)ad_data_len[i], ad_reports[i]->toString(false).c_str());
            }
        }
#endif
    }
    return ad_reports;
}

jau::darray<std::unique_ptr<EInfoReport>> EInfoReport::read_ext_ad_reports(uint8_t const * data, jau::nsize_t const data_length) noexcept {
    jau::nsize_t const num_reports = (jau::nsize_t) data[0];
    jau::darray<std::unique_ptr<EInfoReport>> ad_reports;

    if( 0 == num_reports || num_reports > 0x19 ) {
        DBG_PRINT("EAD-Reports: Invalid reports count: %d", num_reports);
        return ad_reports;
    }
    uint8_t const *limes = data + data_length;
    uint8_t const *i_octets = data + 1;
    uint8_t ad_data_len[0x19];
    jau::nsize_t i;
    const uint64_t timestamp = jau::getCurrentMilliseconds();

    const int seg12_size = 2 + 1 + 6 + 1 + 1 + 1 + 1 + 1 + 2 + 1 + 6 + 1;

    for(i = 0; i < num_reports; i++) {
        ad_reports.push_back( std::make_unique<EInfoReport>() );
        ad_reports[i]->setSource( Source::AD_IND, true /* ext */); // first guess
        ad_reports[i]->setTimestamp(timestamp);

        if( i_octets + seg12_size > limes ) {
            const jau::snsize_t bytes_left = static_cast<jau::snsize_t>(limes - i_octets);
            WARN_PRINT("EAD-Reports: Insufficient data length (1) %zu: report %zu/%zu: min_data_len %zu > bytes-left %zu (Drop)",
                    data_length, i, num_reports, seg12_size, bytes_left);
            ad_reports.pop_back();
            goto errout;
        }

        // seg 1: 2
        {
            const EAD_Event_Type ead_type = static_cast<EAD_Event_Type>(jau::get_uint16(i_octets + 0, jau::lb_endian::little));
            ad_reports[i]->setExtEvtType(ead_type);
            i_octets+=2;
            if( is_set(ead_type, EAD_Event_Type::LEGACY_PDU) ) {
                const AD_PDU_Type ad_type = static_cast<AD_PDU_Type>( ::number(ead_type) );
                ad_reports[i]->setEvtType( ad_type );
                ad_reports[i]->setSource( toSource( ad_type ), true /* ext */);
            } else {
                ad_reports[i]->setSource( toSource( ead_type ), true /* ext */);
            }
        }

        // seg 2: 1
        ad_reports[i]->setADAddressType(*i_octets++);

        // seg 3: 6
        ad_reports[i]->setAddress( jau::le_to_cpu( *((jau::EUI48 const *)i_octets) ) );
        i_octets += 6;

        // seg 4: 1
        // Primary_PHY: 0x01 = LE_1M, 0x03 = LE_CODED (TODO)
        i_octets++;

        // seg 5: 1
        // Sexondary_PHY: 0x00 None, 0x01 = LE_1M, 0x02 = LE_2M, 0x03 = LE_CODED (TODO)
        i_octets++;

        // seg 6: 1
        // Advertising_SID (TODO)
        i_octets++;

        // seg 7: 1
        ad_reports[i]->setTxPower(*const_uint8_to_const_int8_ptr(i_octets));
        i_octets++;

        // seg 8: 1
        ad_reports[i]->setRSSI(*const_uint8_to_const_int8_ptr(i_octets));
        i_octets++;

        // seg 9: 2
        // Periodic_Advertising_Interval (TODO)
        i_octets+=2;

        // seg 10: 1
        // Direct_Address_Type (TODO)
        i_octets++;

        // seg 11: 6
        // Direct_Address (TODO)
        i_octets+=6;

        // seg 12: 1
        ad_data_len[i] = *i_octets++;

        // seg 13: ADV Response Data (EIR)
        if( i_octets + ad_data_len[i] > limes ) {
            const jau::snsize_t bytes_left = static_cast<jau::snsize_t>(limes - i_octets);
            WARN_PRINT("EAD-Reports: Insufficient data length (2) %zu: report %zu/%zu: eir_data_len %zu > bytes-left %zu (Drop)",
                    data_length, i, num_reports, ad_data_len[i], bytes_left);
            ad_reports.pop_back();
            goto errout;
        }
        if( 0 < ad_data_len[i] ) {
            ad_reports[i]->read_data(i_octets, ad_data_len[i]);
            i_octets += ad_data_len[i];
        }
    }

errout:
    {
        const jau::snsize_t bytes_left = static_cast<jau::snsize_t>(limes - i_octets);
        const jau::snsize_t bytes_took = static_cast<jau::snsize_t>(i_octets - data);
        if( 0 > bytes_left ) {
            ERR_PRINT("EAD-Reports: Buffer overflow: %zu reports, bytes[consumed %zu, left %zu, total %zu]",
                    num_reports, bytes_took, bytes_left, data_length);
        }
#ifdef AD_DEBUG
        else {
            DBG_PRINT("EAD-Reports: %zu reports, bytes[consumed %zu, left %zu, total %zu]",
                    num_reports, bytes_took, bytes_left, data_length);
        }
        if( jau::environment::get().debug ) {
            for(i=0; i<num_reports; i++) {
                jau::INFO_PRINT("EAD[%d]: ad_data_length %d, %s\n", (int)i, (int)ad_data_len[i], ad_reports[i]->toString(false).c_str());
            }
        }
#endif
    }
    return ad_reports;
}

// *************************************************
// *************************************************
// *************************************************

