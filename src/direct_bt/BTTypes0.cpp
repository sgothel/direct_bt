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

#include  <algorithm>

#include <jau/debug.hpp>
#include <jau/darray.hpp>

#include "BTTypes0.hpp"

using namespace direct_bt;

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

HCILEOwnAddressType direct_bt::to_HCILEOwnAddressType(const BDAddressType addrType) noexcept {
    switch(addrType) {
        case BDAddressType::BDADDR_LE_PUBLIC:
            return HCILEOwnAddressType::PUBLIC;

        case BDAddressType::BDADDR_LE_RANDOM:
            /** FIXME: Sufficient mapping for adapter put in random address mode? */
            return HCILEOwnAddressType::RANDOM;

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

BLERandomAddressType EUI48::getBLERandomAddressType(const BDAddressType addressType) const noexcept {
    if( BDAddressType::BDADDR_LE_RANDOM != addressType ) {
        return BLERandomAddressType::UNDEFINED;
    }
    const uint8_t high2 = ( b[5] >> 6 ) & 0x03;
    switch( high2 ) {
        case 0x00: return BLERandomAddressType::UNRESOLVABLE_PRIVAT;
        case 0x01: return BLERandomAddressType::RESOLVABLE_PRIVAT;
        case 0x02: return BLERandomAddressType::RESERVED;
        case 0x03: return BLERandomAddressType::STATIC_PUBLIC;
        default: return BLERandomAddressType::UNDEFINED;
    }
}

std::string EUI48Sub::toString() const noexcept {
    // str_len = 2 * len + ( len - 1 )
    // str_len = 3 * len - 1
    // len = ( str_len + 1 ) / 3
    std::string str;
    if( 0 < length ) {
        str.reserve(3 * length - 1);

        for(int i=length-1; 0 <= i; i--) {
            jau::byteHexString(str, b[i], false /* lowerCase */);
            if( 0 < i ) {
                str.push_back(':');
            }
        }
    } else {
        str.push_back(':');
    }
    return str;
}

bool EUI48Sub::scanEUI48Sub(const std::string& str, EUI48Sub& dest, std::string& errmsg) {
    const jau::nsize_t str_len = static_cast<jau::nsize_t>( str.length() );
    dest.clear();

    if( 17 < str_len ) { // not exceeding byte_size
        errmsg.append("EUI48 sub-string must be less or equal length 17 but "+std::to_string(str_len)+": "+str);
        return false;
    }
    const char * str_ptr = str.c_str();
    jau::nsize_t j=0;
    bool exp_colon = false;
    uint8_t b_[6]; // intermediate result high -> low
    while( j+1 < str_len /* && byte_count_ < byte_size */ ) { // min 2 chars left
        const bool is_colon = ':' == str[j];
        if( exp_colon && !is_colon ) {
            errmsg.append("EUI48Sub sub-string not in format '01:02:03:0A:0B:0C', but '"+str+"', colon missing, pos "+std::to_string(j)+", len "+std::to_string(str_len));
            return false;
        } else if( is_colon ) {
            ++j;
            exp_colon = false;
        } else {
            if ( sscanf(str_ptr+j, "%02hhx", &b_[dest.length]) != 1 ) // b_: high->low
            {
                errmsg.append("EUI48Sub sub-string not in format '01:02:03:0A:0B:0C' but '"+str+"', pos "+std::to_string(j)+", len "+std::to_string(str_len));
                return false;
            }
            j += 2;
            ++dest.length;
            exp_colon = true;
        }
    }
    for(j=0; j<dest.length; ++j) { // swap low->high
        dest.b[j] = b_[dest.length-1-j];
    }
    // sscanf provided host data type, in which we store the values,
    // hence no endian conversion
    return true;
}

EUI48Sub::EUI48Sub(const std::string& str) {
    std::string errmsg;
    if( !scanEUI48Sub(str, *this, errmsg) ) {
        throw jau::IllegalArgumentException(errmsg, E_FILE_LINE);
    }
}

EUI48Sub::EUI48Sub(const uint8_t * b_, const jau::nsize_t len_) noexcept {
    length = len_;
    const jau::nsize_t cpsz = std::max<jau::nsize_t>(sizeof(b), len_);
    const jau::nsize_t bzsz = sizeof(b) - cpsz;
    memcpy(b, b_, cpsz);
    if( bzsz > 0 ) {
        bzero(b+cpsz, bzsz);
    }
}

jau::snsize_t EUI48Sub::indexOf(const uint8_t haystack_b[], const jau::nsize_t haystack_length,
                                const uint8_t needle_b[], const jau::nsize_t needle_length) noexcept {
    if( 0 == needle_length ) {
        return 0;
    }
    if( haystack_length < needle_length ) {
        return -1;
    }
    const uint8_t first = needle_b[0];
    const jau::nsize_t outerEnd = haystack_length - needle_length + 1; // exclusive

    for (jau::nsize_t i = 0; i < outerEnd; i++) {
        // find first char of other
        while( haystack_b[i] != first ) {
            if( ++i == outerEnd ) {
                return -1;
            }
        }
        if( i < outerEnd ) { // otherLen chars left to match?
            // continue matching other chars
            const jau::nsize_t innerEnd = i + needle_length; // exclusive
            jau::nsize_t j = i, k=0;
            do {
                if( ++j == innerEnd ) {
                    return i; // gotcha
                }
            } while( haystack_b[j] == needle_b[++k] );
        }
    }
    return -1;
}

std::string EUI48::toString() const noexcept {
    // str_len = 2 * len + ( len - 1 )
    // str_len = 3 * len - 1
    // len = ( str_len + 1 ) / 3
    std::string str;
    str.reserve(17); // 6 * 2 + ( 6 - 1 )

    for(int i=6-1; 0 <= i; i--) {
        jau::byteHexString(str, b[i], false /* lowerCase */);
        if( 0 < i ) {
            str.push_back(':');
        }
    }
    return str;
}

bool EUI48::scanEUI48(const std::string& str, EUI48& dest, std::string& errmsg) {
    if( 17 != str.length() ) {
        errmsg.append("EUI48 string not of length 17 but ");
        errmsg.append(std::to_string(str.length()));
        errmsg.append(": "+str);
        return false;
    }
    if ( sscanf(str.c_str(), "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                &dest.b[5], &dest.b[4], &dest.b[3], &dest.b[2], &dest.b[1], &dest.b[0]) != 6 )
    {
        errmsg.append("EUI48 string not in format '01:02:03:0A:0B:0C' but '"+str+"'");
        return false;
    }
    // sscanf provided host data type, in which we store the values,
    // hence no endian conversion
    return true;
}

EUI48::EUI48(const std::string& str) {
    std::string errmsg;
    if( !scanEUI48(str, *this, errmsg) ) {
        throw jau::IllegalArgumentException(errmsg, E_FILE_LINE);
    }
}

EUI48::EUI48(const uint8_t * b_) noexcept {
    memcpy(b, b_, sizeof(b));
}

static uint8_t _EUI48_ALL_DEVICE[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static uint8_t _EUI48_LOCAL_DEVICE[] = {0x00, 0x00, 0x00, 0xff, 0xff, 0xff};

const EUI48Sub direct_bt::EUI48Sub::ANY_DEVICE; // default ctor is zero bytes!
const EUI48Sub direct_bt::EUI48Sub::ALL_DEVICE( _EUI48_ALL_DEVICE, 6 );
const EUI48Sub direct_bt::EUI48Sub::LOCAL_DEVICE( _EUI48_LOCAL_DEVICE, 6 );

const EUI48 direct_bt::EUI48::ANY_DEVICE; // default ctor is zero bytes!
const EUI48 direct_bt::EUI48::ALL_DEVICE( _EUI48_ALL_DEVICE );
const EUI48 direct_bt::EUI48::LOCAL_DEVICE( _EUI48_LOCAL_DEVICE );

const BDAddressAndType direct_bt::BDAddressAndType::ANY_BREDR_DEVICE(EUI48::ANY_DEVICE, BDAddressType::BDADDR_BREDR);
const BDAddressAndType direct_bt::BDAddressAndType::ANY_DEVICE(EUI48::ANY_DEVICE, BDAddressType::BDADDR_UNDEFINED);

std::string BDAddressAndType::toString() const noexcept {
    const BLERandomAddressType leRandomAddressType = getBLERandomAddressType();
    std::string leaddrtype;
    if( BLERandomAddressType::UNDEFINED != leRandomAddressType ) {
        leaddrtype = ", random "+to_string(leRandomAddressType);
    }
    return "["+address.toString()+", "+to_string(type)+leaddrtype+"]";
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

#define LEFEATURES_ENUM(X) \
    X(LE_Features,NONE) \
    X(LE_Features,LE_Encryption) \
    X(LE_Features,Conn_Param_Req_Proc) \
    X(LE_Features,Ext_Rej_Ind) \
    X(LE_Features,SlaveInit_Feat_Exchg) \
    X(LE_Features,LE_Ping) \
    X(LE_Features,LE_Data_Pkt_Len_Ext) \
    X(LE_Features,LL_Privacy) \
    X(LE_Features,Ext_Scan_Filter_Pol) \
    X(LE_Features,LE_2M_PHY) \
    X(LE_Features,Stable_Mod_Idx_Tx) \
    X(LE_Features,Stable_Mod_Idx_Rx) \
    X(LE_Features,LE_Coded_PHY) \
    X(LE_Features,LE_Ext_Adv) \
    X(LE_Features,LE_Per_Adv) \
    X(LE_Features,Chan_Sel_Algo_2) \
    X(LE_Features,LE_Pwr_Cls_1) \
    X(LE_Features,Min_Num_Used_Chan_Proc) \
    X(LE_Features,Conn_CTE_Req) \
    X(LE_Features,Conn_CTE_Res) \
    X(LE_Features,ConnLess_CTE_Tx) \
    X(LE_Features,ConnLess_CTE_Rx) \
    X(LE_Features,AoD) \
    X(LE_Features,AoA) \
    X(LE_Features,Rx_Const_Tone_Ext) \
    X(LE_Features,Per_Adv_Sync_Tx_Sender) \
    X(LE_Features,Per_Adv_Sync_Tx_Rec) \
    X(LE_Features,Zzz_Clk_Acc_Upd) \
    X(LE_Features,Rem_Pub_Key_Val) \
    X(LE_Features,Conn_Iso_Stream_Master) \
    X(LE_Features,Conn_Iso_Stream_Slave) \
    X(LE_Features,Iso_Brdcst) \
    X(LE_Features,Sync_Rx) \
    X(LE_Features,Iso_Chan) \
    X(LE_Features,LE_Pwr_Ctrl_Req) \
    X(LE_Features,LE_Pwr_Chg_Ind) \
    X(LE_Features,LE_Path_Loss_Mon)

static std::string _getLEFeaturesBitStr(const LE_Features bit) noexcept {
    switch(bit) {
    LEFEATURES_ENUM(CASE2_TO_STRING)
        default: ; // fall through intended
    }
    return "??? "+jau::to_hexstring(number(bit));
}

std::string direct_bt::to_string(const LE_Features mask) noexcept {
    const uint64_t one = 1;
    bool has_pre = false;
    std::string out("[");
    for(int i=0; i<36; i++) {
        const LE_Features settingBit = static_cast<LE_Features>( one << i );
        if( LE_Features::NONE != ( mask & settingBit ) ) {
            if( has_pre ) { out.append(", "); }
            out.append(_getLEFeaturesBitStr(settingBit));
            has_pre = true;
        }
    }
    out.append("]");
    return out;
}

// *************************************************
// *************************************************
// *************************************************

#define LE_PHYs_ENUM(X) \
    X(LE_PHYs,NONE) \
    X(LE_PHYs,LE_1M) \
    X(LE_PHYs,LE_2M) \
    X(LE_PHYs,LE_CODED)

static std::string _getLE_PHYsBitStr(const LE_PHYs bit) noexcept {
    switch(bit) {
    LE_PHYs_ENUM(CASE2_TO_STRING)
        default: ; // fall through intended
    }
    return "??? "+jau::to_hexstring(number(bit));
}

std::string direct_bt::to_string(const LE_PHYs mask) noexcept {
    const uint8_t one = 1;
    bool has_pre = false;
    std::string out("[");
    for(int i=0; i<4; i++) {
        const LE_PHYs settingBit = static_cast<LE_PHYs>( one << i );
        if( LE_PHYs::NONE != ( mask & settingBit ) ) {
            if( has_pre ) { out.append(", "); }
            out.append(_getLE_PHYsBitStr(settingBit));
            has_pre = true;
        }
    }
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

#define EAD_Event_Type_ENUM(X) \
    X(EAD_Event_Type,NONE) \
    X(EAD_Event_Type,CONN_ADV) \
    X(EAD_Event_Type,SCAN_ADV) \
    X(EAD_Event_Type,DIR_ADV) \
    X(EAD_Event_Type,SCAN_RSP) \
    X(EAD_Event_Type,LEGACY_PDU) \
    X(EAD_Event_Type,DATA_B0) \
    X(EAD_Event_Type,DATA_B1)

static std::string _getEAD_Event_TypeBitStr(const EAD_Event_Type bit) noexcept {
    switch(bit) {
    EAD_Event_Type_ENUM(CASE2_TO_STRING)
        default: ; // fall through intended
    }
    return "??? "+jau::to_hexstring(number(bit));
}

std::string direct_bt::to_string(const EAD_Event_Type mask) noexcept {
    const uint16_t one = 1;
    bool has_pre = false;
    std::string out("[");
    for(int i=0; i<8; i++) {
        const EAD_Event_Type settingBit = static_cast<EAD_Event_Type>( one << i );
        if( EAD_Event_Type::NONE != ( mask & settingBit ) ) {
            if( has_pre ) { out.append(", "); }
            out.append(_getEAD_Event_TypeBitStr(settingBit));
            has_pre = true;
        }
    }
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

ManufactureSpecificData::ManufactureSpecificData(uint16_t const company_) noexcept
: company(company_), companyName(std::string(bt_compidtostr(company_))), data(/* intentional zero sized */) {
}

ManufactureSpecificData::ManufactureSpecificData(uint16_t const company_, uint8_t const * const data_, jau::nsize_t const data_len) noexcept
: company(company_), companyName(std::string(bt_compidtostr(company_))), data(data_, data_len) {
}

std::string ManufactureSpecificData::toString() const noexcept {
  std::string out("MSD[company[");
  out.append(std::to_string(company)+" "+companyName);
  out.append("], data["+data.toString()+"]]");
  return out;
}

// *************************************************
// *************************************************
// *************************************************

#define GAPFLAGS_ENUM(X) \
    X(GAPFlags,NONE) \
    X(GAPFlags,LE_Ltd_Disc) \
    X(GAPFlags,LE_Gen_Disc) \
    X(GAPFlags,BREDR_UNSUP) \
    X(GAPFlags,Dual_SameCtrl) \
    X(GAPFlags,Dual_SameHost) \
    X(GAPFlags,RESERVED1) \
    X(GAPFlags,RESERVED2) \
    X(GAPFlags,RESERVED3)

static std::string _getGAPFlagBitStr(const GAPFlags bit) noexcept {
    switch(bit) {
    GAPFLAGS_ENUM(CASE2_TO_STRING)
        default: ; // fall through intended
    }
    return "??? "+jau::to_hexstring(number(bit));
}

std::string direct_bt::to_string(const GAPFlags v) noexcept {
    const int v_i = static_cast<int>(v);
    bool has_pre = false;
    std::string out("[");
    for(int i=0; i<8; i++) {
        const int settingBit = ( 1 << i );
        if( 0 != ( v_i & settingBit ) ) {
            if( has_pre ) { out.append(", "); }
            out.append( _getGAPFlagBitStr( static_cast<GAPFlags>(settingBit) ) );
            has_pre = true;
        }
    }
    out.append("]");
    return out;
}

// *************************************************
// *************************************************
// *************************************************

#define EIRDATATYPE_ENUM(X) \
    X(EIRDataType,NONE) \
    X(EIRDataType,EVT_TYPE) \
    X(EIRDataType,EXT_EVT_TYPE) \
    X(EIRDataType,BDADDR_TYPE) \
    X(EIRDataType,BDADDR) \
    X(EIRDataType,FLAGS) \
    X(EIRDataType,NAME) \
    X(EIRDataType,NAME_SHORT) \
    X(EIRDataType,RSSI) \
    X(EIRDataType,TX_POWER) \
    X(EIRDataType,MANUF_DATA) \
    X(EIRDataType,DEVICE_CLASS) \
    X(EIRDataType,APPEARANCE) \
    X(EIRDataType,HASH) \
    X(EIRDataType,RANDOMIZER) \
    X(EIRDataType,DEVICE_ID) \
    X(EIRDataType,SERVICE_UUID) \
    X(EIRDataType,ALL)

static std::string _getEIRDataBitStr(const EIRDataType bit) noexcept {
    switch(bit) {
    EIRDATATYPE_ENUM(CASE2_TO_STRING)
        default: ; // fall through intended
    }
    return "??? "+jau::to_hexstring(number(bit));
}

std::string direct_bt::to_string(const EIRDataType mask) noexcept {
    const uint32_t one = 1;
    bool has_pre = false;
    std::string out("[");
    for(int i=0; i<32; i++) {
        const EIRDataType settingBit = static_cast<EIRDataType>( one << i );
        if( EIRDataType::NONE != ( mask & settingBit ) ) {
            if( has_pre ) { out.append(", "); }
            out.append(_getEIRDataBitStr(settingBit));
            has_pre = true;
        }
    }
    out.append("]");
    return out;
}

// *************************************************
// *************************************************
// *************************************************

std::string direct_bt::to_string(EInfoReport::Source source) noexcept {
    switch (source) {
        case EInfoReport::Source::NA: return "N/A";
        case EInfoReport::Source::AD: return "AD";
        case EInfoReport::Source::EAD: return "EAD";
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

void EInfoReport::setManufactureSpecificData(uint16_t const company, uint8_t const * const data, int const data_len) noexcept {
    if( nullptr == data || 0 >= data_len ) {
        msd = std::make_shared<ManufactureSpecificData>(company);
    } else {
        msd = std::make_shared<ManufactureSpecificData>(company, data, data_len);
    }
    set(EIRDataType::MANUF_DATA);
}
void EInfoReport::setManufactureSpecificData(const ManufactureSpecificData& msd_) noexcept {
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

void EInfoReport::addService(const std::shared_ptr<const uuid_t>& uuid) noexcept
{
    auto begin = services.begin();
    auto it = std::find_if(begin, services.end(), [&](std::shared_ptr<const uuid_t> const& p) {
        return *p == *uuid;
    });
    if ( it == std::end(services) ) {
        services.push_back(uuid);
        set(EIRDataType::SERVICE_UUID);
    }
}
void EInfoReport::addService(const uuid_t& uuid) noexcept {
    addService( uuid.clone() );
}

std::string EInfoReport::eirDataMaskToString() const noexcept {
    return std::string("DataSet"+ direct_bt::to_string(eir_data_mask) );
}
std::string EInfoReport::toString(const bool includeServices) const noexcept {
    std::string msdstr = nullptr != msd ? msd->toString() : "MSD[null]";
    std::string out("EInfoReport::"+to_string(source)+
                    "[address["+address.toString()+", "+to_string(getAddressType())+"/"+std::to_string(ad_address_type)+
                    "], name['"+name+"'/'"+name_short+"'], "+eirDataMaskToString()+
                    ", type[evt "+to_string(evt_type)+", ead "+to_string(ead_type)+
                    "], flags"+to_string(flags)+
                    ", rssi "+std::to_string(rssi)+
                    ", tx-power "+std::to_string(tx_power)+
                    ", dev-class "+jau::to_hexstring(device_class)+
                    ", appearance "+jau::to_hexstring(static_cast<uint16_t>(appearance))+" ("+to_string(appearance)+
                    "), hash["+hash.toString()+
                    "], randomizer["+randomizer.toString()+
                    "], device-id[source "+jau::to_hexstring(did_source)+
                    ", vendor "+jau::to_hexstring(did_vendor)+
                    ", product "+jau::to_hexstring(did_product)+
                    ", version "+jau::to_hexstring(did_version)+
                    "], "+msdstr+"]");

    if( includeServices && services.size() > 0 ) {
        out.append("\n");
        for(auto it = services.begin(); it != services.end(); it++) {
            std::shared_ptr<const uuid_t> p = *it;
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
                       [](const std::shared_ptr<const uuid_t>&a, const std::shared_ptr<const uuid_t>&b)
                       -> bool { return *a == *b; } );
}

std::string EInfoReport::getDeviceIDModalias() const noexcept {
    char *cstr = NULL;
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
        if( NULL != cstr ) {
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
                for(int j=0; j<elem_len/2; j++) {
                    const std::shared_ptr<const uuid_t> uuid( std::make_shared<const uuid16_t>(elem_data, j*2, true) );
                    addService( uuid );
                }
                break;

            case GAP_T::UUID32_INCOMPLETE:
                [[fallthrough]];
            case GAP_T::UUID32_COMPLETE:
                for(int j=0; j<elem_len/4; j++) {
                    const std::shared_ptr<const uuid_t> uuid( std::make_shared<const uuid32_t>(elem_data, j*4, true) );
                    addService( uuid );
                }
                break;

            case GAP_T::UUID128_INCOMPLETE:
                [[fallthrough]];
            case GAP_T::UUID128_COMPLETE:
                for(int j=0; j<elem_len/16; j++) {
                    const std::shared_ptr<const uuid_t> uuid( std::make_shared<const uuid128_t>(elem_data, j*16, true) );
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
                    setAppearance(static_cast<AppearanceCat>( jau::get_uint16(elem_data, 0, true /* littleEndian */) ));
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
                    const uint16_t company = jau::get_uint16(elem_data, 0, true /* littleEndian */);
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

#define _WARN_OOB(a) WARN_PRINT("%s: Out of buffer: count %zd + 1 + ad_sz %zd > data_len %zd -> drop %s\n", (a), count, ad_sz, data_length, toString(true).c_str());

jau::nsize_t EInfoReport::write_data(EIRDataType write_mask, uint8_t * data, jau::nsize_t const data_length) const noexcept {
    jau::nsize_t count = 0;
    uint8_t * data_i = data;
    const EIRDataType mask = write_mask & eir_data_mask;

    if( isEIRDataTypeSet(mask, EIRDataType::FLAGS) ) {
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
    if( isEIRDataTypeSet(mask, EIRDataType::NAME) ) {
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
    } else if( isEIRDataTypeSet(mask, EIRDataType::NAME_SHORT) ) {
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
    if( isEIRDataTypeSet(mask, EIRDataType::MANUF_DATA) && nullptr != msd ) {
        const jau::nsize_t msd_data_sz = msd->getData().getSize();
        const jau::nsize_t ad_sz = 1 + 2 + msd_data_sz;
        if( ( count + 1 + ad_sz ) > data_length ) {
            _WARN_OOB("MANUF_DATA");
            return count;
        }
        count    += ad_sz + 1;
        *data_i++ = ad_sz;
        *data_i++ = direct_bt::number( GAP_T::MANUFACTURE_SPECIFIC );
        jau::put_uint16(data_i, 0, msd->getCompany(), true /* littleEndian */);
        data_i += 2;
        if( 0 < msd_data_sz ) {
            memcpy(data_i, msd->getData().get_ptr(), msd_data_sz);
            data_i +=msd_data_sz;
        }
    }
    if( isEIRDataTypeSet(mask, EIRDataType::SERVICE_UUID) ) {
        jau::darray<std::shared_ptr<const uuid_t>> uuid16s, uuid32s, uuid128s;
        for(auto it = services.begin(); it != services.end(); it++) {
            std::shared_ptr<const uuid_t> p = *it;
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
            *data_i++ = direct_bt::number(GAP_T::UUID16_COMPLETE);
            for(auto it = uuid16s.begin(); it != uuid16s.end(); it++) {
                std::shared_ptr<const uuid_t> p = *it;
                data_i += p->put(data_i, 0, true /* le */);
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
            *data_i++ = direct_bt::number(GAP_T::UUID32_COMPLETE);
            for(auto it = uuid32s.begin(); it != uuid32s.end(); it++) {
                std::shared_ptr<const uuid_t> p = *it;
                data_i += p->put(data_i, 0, true /* le */);
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
            *data_i++ = direct_bt::number(GAP_T::UUID128_COMPLETE);
            for(auto it = uuid128s.begin(); it != uuid128s.end(); it++) {
                std::shared_ptr<const uuid_t> p = *it;
                data_i += p->put(data_i, 0, true /* le */);
            }
        }
    }
    // TODO:
    // if( isEIRDataTypeSet(mask, EIRDataType::DEVICE_CLASS) ) {}
    // if( isEIRDataTypeSet(mask, EIRDataType::APPEARANCE) ) {}
    // if( isEIRDataTypeSet(mask, EIRDataType::HASH) ) {}
    // if( isEIRDataTypeSet(mask, EIRDataType::RANDOMIZER) ) {}
    // if( isEIRDataTypeSet(mask, EIRDataType::DEVICE_ID) ) {}

    // Mark end of significant part
    if( ( count + 1 ) <= data_length ) {
        count    += 1;
        *data_i++ = 0; // zero length
    }

    return count;
}
// #define AD_DEBUG 1

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
    jau::nsize_t segment_count = 5 * num_reports; // excluding data segment
    jau::nsize_t read_segments = 0;
    jau::nsize_t i;
    const uint64_t timestamp = jau::getCurrentMilliseconds();

    for(i = 0; i < num_reports && i_octets < limes; i++) {
        ad_reports.push_back( std::make_unique<EInfoReport>() );
        ad_reports[i]->setSource(Source::AD);
        ad_reports[i]->setTimestamp(timestamp);
        ad_reports[i]->setEvtType(static_cast<AD_PDU_Type>(*i_octets++));
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets < limes; i++) {
        ad_reports[i]->setADAddressType(*i_octets++);
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets + 5 < limes; i++) {
        ad_reports[i]->setAddress( *((EUI48 const *)i_octets) );
        i_octets += 6;
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets < limes; i++) {
        ad_data_len[i] = *i_octets++;
        read_segments++;
    }
    for(i = 0; i < num_reports; i++) { // adjust segement_count
        if( 0 < ad_data_len[i] ) {
            segment_count++;
        }
    }
    for(i = 0; i < num_reports && i_octets + ad_data_len[i] <= limes; i++) {
        if( 0 < ad_data_len[i] ) {
            ad_reports[i]->read_data(i_octets, ad_data_len[i]);
            i_octets += ad_data_len[i];
            read_segments++;
        }
    }
    for(i = 0; i < num_reports && i_octets < limes; i++) {
        ad_reports[i]->setRSSI(*const_uint8_to_const_int8_ptr(i_octets));
        i_octets++;
        read_segments++;
    }
    const jau::snsize_t bytes_left = static_cast<jau::snsize_t>(limes - i_octets);

    if( segment_count != read_segments || 0 > bytes_left ) {
        if( 0 > bytes_left ) {
            ERR_PRINT("EAD-Reports: Buffer overflow\n");
        } else {
            WARN_PRINT("EAD-Reports: Incomplete data segments\n");
        }
        WARN_PRINT("AD-Reports: %zu reports within %zu bytes: Segment read %zu < %zu, data-ptr %zd bytes to limes\n",
                num_reports, data_length, read_segments, segment_count, bytes_left);
        for(i=0; i<num_reports; i++) {
            WARN_PRINT("EAD[%d]: ad_data_length %d, %s\n", (int)i, (int)ad_data_len[i], ad_reports[i]->toString(false).c_str());
        }
    }
#if AD_DEBUG
    else {
        DBG_PRINT("AD-Reports: %zu reports within %zu bytes: Segment read %zu (%zu), data-ptr %zd bytes to limes\n",
                num_reports, data_length, read_segments, segment_count, bytes_left);
        for(i=0; i<num_reports; i++) {
            DBG_PRINT("EAD[%d]: ad_data_length %d, %s\n", (int)i, (int)ad_data_len[i], ad_reports[i]->toString(false).c_str());
        }
    }
#endif /* AD_DEBUG */
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
    jau::nsize_t segment_count = 12 * num_reports; // excluding data segment
    jau::nsize_t read_segments = 0;
    jau::nsize_t i;
    const uint64_t timestamp = jau::getCurrentMilliseconds();

    for(i = 0; i < num_reports && i_octets < limes; i++) {
        ad_reports.push_back( std::make_unique<EInfoReport>() );
        ad_reports[i]->setSource(Source::EAD);
        ad_reports[i]->setTimestamp(timestamp);
        const EAD_Event_Type ead_type_ = static_cast<EAD_Event_Type>(jau::get_uint16(i_octets, 0, true /* littleEndian */));
        ad_reports[i]->setExtEvtType(ead_type_);
        i_octets+=2;
        if( isEAD_Event_TypeSet(ead_type_, EAD_Event_Type::LEGACY_PDU) ) {
            ad_reports[i]->setEvtType(static_cast<AD_PDU_Type>(number(ead_type_)));
        }
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets < limes; i++) {
        ad_reports[i]->setADAddressType(*i_octets++);
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets + 5 < limes; i++) {
        ad_reports[i]->setAddress( *((EUI48 const *)i_octets) );
        i_octets += 6;
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets < limes; i++) {
        // Primary_PHY: 0x01 = LE_1M, 0x03 = LE_CODED
        i_octets++;
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets < limes; i++) {
        // Sexondary_PHY: 0x00 None, 0x01 = LE_1M, 0x02 = LE_2M, 0x03 = LE_CODED
        i_octets++;
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets < limes; i++) {
        // Advertising_SID
        i_octets++;
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets < limes; i++) {
        ad_reports[i]->setTxPower(*const_uint8_to_const_int8_ptr(i_octets));
        i_octets++;
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets < limes; i++) {
        ad_reports[i]->setRSSI(*const_uint8_to_const_int8_ptr(i_octets));
        i_octets++;
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets < limes; i++) {
        // Periodic_Advertising_Interval
        i_octets+=2;
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets < limes; i++) {
        // Direct_Address_Type
        i_octets++;
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets < limes; i++) {
        // Direct_Address
        i_octets+=6;
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets < limes; i++) {
        ad_data_len[i] = *i_octets++;
        read_segments++;
    }
    for(i = 0; i < num_reports; i++) { // adjust segement_count
        if( 0 < ad_data_len[i] ) {
            segment_count++;
        }
    }
    for(i = 0; i < num_reports && i_octets + ad_data_len[i] <= limes; i++) {
        if( 0 < ad_data_len[i] ) {
            ad_reports[i]->read_data(i_octets, ad_data_len[i]);
            i_octets += ad_data_len[i];
            read_segments++;
        }
    }
    const jau::snsize_t bytes_left = static_cast<jau::snsize_t>(limes - i_octets);

    if( segment_count != read_segments || 0 > bytes_left ) {
        if( 0 > bytes_left ) {
            ERR_PRINT("EAD-Reports: Buffer overflow\n");
        } else {
            WARN_PRINT("EAD-Reports: Incomplete data segments\n");
        }
        WARN_PRINT("EAD-Reports: %zu reports within %zu bytes: Segment read %zu < %zu, data-ptr %zd bytes to limes\n",
                num_reports, data_length, read_segments, segment_count, bytes_left);
        for(i=0; i<num_reports; i++) {
            WARN_PRINT("EAD[%d]: ad_data_length %d, %s\n", (int)i, (int)ad_data_len[i], ad_reports[i]->toString(false).c_str());
        }
    }
#if AD_DEBUG
    else {
        DBG_PRINT("EAD-Reports: %zu reports within %zu bytes: Segment read %zu (%zu), data-ptr %zd bytes to limes\n",
                num_reports, data_length, read_segments, segment_count, bytes_left);
        for(i=0; i<num_reports; i++) {
            DBG_PRINT("EAD[%d]: ad_data_length %d, %s\n", (int)i, (int)ad_data_len[i], ad_reports[i]->toString(false).c_str());
        }
    }
#endif /* AD_DEBUG */
    return ad_reports;
}

// *************************************************
// *************************************************
// *************************************************

