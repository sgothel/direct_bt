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
    }
    return str;
}

EUI48Sub::EUI48Sub(const std::string str) {
    const jau::nsize_t str_len = static_cast<jau::nsize_t>( str.length() );
    length = 0;

    if( 17 < str_len ) { // not exceeding byte_size
        throw jau::IllegalArgumentException("EUI48 sub-string must be less or equal length 17 but "+std::to_string(str_len)+": "+str, E_FILE_LINE);
    }
    const char * str_ptr = str.c_str();
    jau::nsize_t j=0;
    uint8_t b_[6]; // intermediate result high -> low
    while( j+1 < str_len /* && byte_count_ < byte_size */ ) { // min 2 chars left
        if( ':' == str[j] ) {
            ++j;
        } else {
            if ( sscanf(str_ptr+j, "%02hhx", &b_[length]) != 1 ) // b_: high->low
            {
                std::string msg("EUI48Sub sub-string not in format '01:02:03:0A:0B:0C' but '"+str+"', pos "+std::to_string(j)+", len "+std::to_string(str_len));
                throw jau::IllegalArgumentException(msg, E_FILE_LINE);
            }
            j += 2;
            ++length;
        }
    }
    for(j=0; j<length; ++j) { // swap low->high
        b[j] = b_[length-1-j];
    }
    // sscanf provided host data type, in which we store the values,
    // hence no endian conversion
}

EUI48Sub::EUI48Sub(const uint8_t * b_, const jau::nsize_t len_) noexcept {
    length = len_;
    memcpy(b, b_, std::max<jau::nsize_t>(sizeof(b), len_));
}

jau::snsize_t EUI48::indexOf(const EUI48Sub& other) const noexcept {
    if( 0 == other.length ) {
        return 0;
    }
    const uint8_t first = other.b[0];
    const jau::nsize_t outerEnd = 6 - other.length + 1; // exclusive

    for (jau::nsize_t i = 0; i < outerEnd; i++) {
        // find first char of other
        while( b[i] != first ) {
            if( ++i == outerEnd ) {
                return -1;
            }
        }
        if( i < outerEnd ) { // otherLen chars left to match?
            // continue matching other chars
            const jau::nsize_t innerEnd = i + other.length; // exclusive
            jau::nsize_t j = i, k=0;
            do {
                if( ++j == innerEnd ) {
                    return i; // gotcha
                }
            } while( b[j] == other.b[++k] );
        }
    }
    return -1;
}

bool EUI48::contains(const EUI48Sub& other) const noexcept {
    return 0 <= indexOf(other);
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

EUI48::EUI48(const std::string str) {
    if( 17 != str.length() ) {
        std::string msg("EUI48 string not of length 17 but ");
        msg.append(std::to_string(str.length()));
        msg.append(": "+str);
        throw jau::IllegalArgumentException(msg, E_FILE_LINE);
    }
    if ( sscanf(str.c_str(), "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                     &b[5], &b[4], &b[3], &b[2], &b[1], &b[0]) != 6 )
    {
        std::string msg("EUI48 string not in format '01:02:03:0A:0B:0C' but '"+str+"'");
        throw jau::IllegalArgumentException(msg, E_FILE_LINE);
    }

    // sscanf provided host data type, in which we store the values,
    // hence no endian conversion
}

EUI48::EUI48(const uint8_t * b_) noexcept {
    memcpy(b, b_, sizeof(b));
}

const EUI48 direct_bt::EUI48::ANY_DEVICE; // default ctor is zero bytes!
static uint8_t _EUI48_ALL_DEVICE[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static uint8_t _EUI48_LOCAL_DEVICE[] = {0x00, 0x00, 0x00, 0xff, 0xff, 0xff};
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
    X(ADV_UNDEFINED)

#define AD_PDU_Type_CASE_TO_STRING(V) case AD_PDU_Type::V: return #V;

std::string direct_bt::to_string(const AD_PDU_Type v) noexcept {
    switch(v) {
        AD_PDU_Type_ENUM(AD_PDU_Type_CASE_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown AD_PDU_Type "+jau::to_hexstring(number(v));
}

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

#define EIRDATATYPE_ENUM(X) \
    X(EIRDataType,NONE) \
    X(EIRDataType,EVT_TYPE) \
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
    X(EIRDataType,SERVICE_UUID)

static std::string _getEIRDataBitStr(const EIRDataType bit) noexcept {
    switch(bit) {
    EIRDATATYPE_ENUM(CASE2_TO_STRING)
        default: ; // fall through intended
    }
    return "Unknown EIRDataType Bit "+jau::to_hexstring(number(bit));
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

void EInfoReport::setShortName(const uint8_t *buffer, int buffer_len) noexcept {
    name_short = jau::get_string(buffer, buffer_len, 30);
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

void EInfoReport::setDeviceID(const uint16_t source_, const uint16_t vendor, const uint16_t product, const uint16_t version) noexcept {
    did_source = source_;
    did_vendor = vendor;
    did_product = product;
    did_version = version;
    set(EIRDataType::DEVICE_ID);
}

void EInfoReport::addService(std::shared_ptr<uuid_t> const &uuid) noexcept
{
    auto begin = services.begin();
    auto it = std::find_if(begin, services.end(), [&](std::shared_ptr<uuid_t> const& p) {
        return *p == *uuid;
    });
    if ( it == std::end(services) ) {
        services.push_back(uuid);
    }
}

std::string EInfoReport::eirDataMaskToString() const noexcept {
    return std::string("DataSet"+ direct_bt::to_string(eir_data_mask) );
}
std::string EInfoReport::toString(const bool includeServices) const noexcept {
    std::string msdstr = nullptr != msd ? msd->toString() : "MSD[null]";
    std::string out("EInfoReport::"+to_string(source)+
                    "[address["+address.toString()+", "+to_string(getAddressType())+"/"+std::to_string(ad_address_type)+
                    "], name['"+name+"'/'"+name_short+"'], "+eirDataMaskToString()+
                    ", evt-type "+to_string(evt_type)+", rssi "+std::to_string(rssi)+
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
            std::shared_ptr<uuid_t> p = *it;
            out.append("  ").append(p->toUUID128String()).append(", ").append(std::to_string(static_cast<int>(p->getTypeSize()))).append(" bytes\n");
        }
    }
    return out;
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
                    setFlags(*const_uint8_to_const_int8_ptr(elem_data));
                }
                break;
            case GAP_T::UUID16_INCOMPLETE:
            case GAP_T::UUID16_COMPLETE:
                for(int j=0; j<elem_len/2; j++) {
                    const std::shared_ptr<uuid_t> uuid( std::make_shared<uuid16_t>(elem_data, j*2, true) );
                    addService(std::move(uuid));
                }
                break;
            case GAP_T::UUID32_INCOMPLETE:
            case GAP_T::UUID32_COMPLETE:
                for(int j=0; j<elem_len/4; j++) {
                    const std::shared_ptr<uuid_t> uuid( std::make_shared<uuid32_t>(elem_data, j*4, true) );
                    addService(std::move(uuid));
                }
                break;
            case GAP_T::UUID128_INCOMPLETE:
            case GAP_T::UUID128_COMPLETE:
                for(int j=0; j<elem_len/16; j++) {
                    const std::shared_ptr<uuid_t> uuid( std::make_shared<uuid128_t>(elem_data, j*16, true) );
                    addService(std::move(uuid));
                }
                break;
            case GAP_T::NAME_LOCAL_SHORT:
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
            case GAP_T::SOLICIT_UUID128:
            case GAP_T::SVC_DATA_UUID16:
            case GAP_T::PUB_TRGT_ADDR:
            case GAP_T::RND_TRGT_ADDR:
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
            case GAP_T::SVC_DATA_UUID32:
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
                fprintf(stderr, "%s-Element @ [%d/%d]: Warning: Unhandled type 0x%.2X with %d bytes net\n",
                        to_string(source).c_str(), offset, data_length, elem_type, elem_len);
                break;
        }
    }
    return count;
}

jau::darray<std::shared_ptr<EInfoReport>> EInfoReport::read_ad_reports(uint8_t const * data, jau::nsize_t const data_length) noexcept {
    jau::nsize_t const num_reports = (jau::nsize_t) data[0];
    jau::darray<std::shared_ptr<EInfoReport>> ad_reports;

    if( 0 == num_reports || num_reports > 0x19 ) {
        DBG_PRINT("AD-Reports: Invalid reports count: %d", num_reports);
        return ad_reports;
    }
    uint8_t const *limes = data + data_length;
    uint8_t const *i_octets = data + 1;
    uint8_t ad_data_len[0x19];
    const jau::nsize_t segment_count = 6;
    jau::nsize_t read_segments = 0;
    jau::nsize_t i;
    const uint64_t timestamp = jau::getCurrentMilliseconds();

    for(i = 0; i < num_reports && i_octets < limes; i++) {
        ad_reports.push_back( std::make_shared<EInfoReport>() );
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
    for(i = 0; i < num_reports && i_octets + ad_data_len[i] < limes; i++) {
        ad_reports[i]->read_data(i_octets, ad_data_len[i]);
        i_octets += ad_data_len[i];
        read_segments++;
    }
    for(i = 0; i < num_reports && i_octets < limes; i++) {
        ad_reports[i]->setRSSI(*const_uint8_to_const_int8_ptr(i_octets));
        i_octets++;
        read_segments++;
    }
    const jau::nsize_t bytes_left = static_cast<jau::nsize_t>(limes - i_octets);

    if( segment_count != read_segments ) {
        WARN_PRINT("AD-Reports: Incomplete %zu reports within %zu bytes: Segment read %zu < %zu, data-ptr %zu bytes to limes\n",
                num_reports, data_length, read_segments, segment_count, bytes_left);
    }
    return ad_reports;
}

// *************************************************
// *************************************************
// *************************************************

